#ifndef CONTROLS_H
#define CONTROLS_H

#include <vector>
#include <functional>

// TODO: rework with interrupts

class HardwareControl {
  friend class HardwareControls;
protected:
  virtual void update() = 0;
  int pin;
public:
  HardwareControl(int pin) : pin(pin) {};
  virtual ~HardwareControl() {};
};

/* ------------------ */

typedef std::function<void(uint32_t)> DialHandler;

class AnalogDial : public HardwareControl {
  DialHandler changeHandler = [](uint32_t){};

  uint32_t lastValue = UINT32_MAX;
  unsigned long lastChange;
  bool firstUpdate = true; // always call handler on first update to help set initial values

  void update() {
    uint32_t value;
    if (readValueFunc) {
      value = (*readValueFunc)();
    } else {
      value = analogRead(pin);
    }
    // potentiometer reads are noisy, jitter may be around ±30 or so. 
    // Wait for significant change to notify the handler, but then still allow smooth updates as the pot is turned
    bool significantChange = abs((int)lastValue - (int)value) > updateThreshold;
    bool recentSignificantChange = (millis() - lastChange < smoothUpdateDuration);
    bool endpointsChange = lastValue != value && (value == 0 || value == maxValue);
    
    if (significantChange || recentSignificantChange || endpointsChange || firstUpdate) {
      // logf("pot update: significantChange = %i [%u - %u > %u], recentSignificantChange = %i, endpointsChange = %i", significantChange, lastValue, value, updateThreshold, recentSignificantChange, endpointsChange);
      if (significantChange || endpointsChange) {
        lastChange = millis();
        lastValue = value;
      }
      changeHandler(value);
    }
    firstUpdate = false;
  }

public:
  unsigned updateThreshold = 40; // Value threshold to suppress jitter in the thumbdial
  unsigned smoothUpdateDuration = 500; // Duration (ms) to call handler on every change after threshold is met

  uint32_t (*readValueFunc)(void) = NULL;
  uint32_t maxValue = 1023;

  AnalogDial(int pin) : HardwareControl(pin) {
  }

  void onChange(DialHandler handler) {
    changeHandler = handler;
  }
};

/* ----------------------- */

typedef std::function<void(void)> ButtonHandler;

class SPSTButton : public HardwareControl {
  typedef enum {
    singlePress,      // 0
    doublePress,      // 1
    longPress,        // 2
    veryLongPress,    // 3
    doubleLongPress,  // 4
    buttonDown,       // 5
    buttonUp,         // 6
    handlerTypeCount,
  } HandlerType;

  uint32_t buttonDownTime = 0;
  uint32_t singlePressTime = 0;
  bool waitForButtonUpLongPress = false;
  bool waitForButtonUpVeryLongPress = false;
  
  bool didInit = false;
  bool seenFirstButtonUp = false;
  bool loggedSeenFirstButtonUp = false;
  
  ButtonHandler *handlers[handlerTypeCount] = {0};

  virtual void initPin(int pin) {
    pinMode(pin, pressedState == LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  }

  void onHandler(HandlerType type, ButtonHandler handler) {
    // we go through this handlers dance in order to not spend memory on expensive std::functions for handlers not in use.
    if (handlers[type]) {
      delete handlers[type];
    }
    handlers[type] = new ButtonHandler(handler);
  }

  void doHandler(HandlerType type) {
    logdf("Do button handler: %i", type);
    if (handlers[type])
      (*(handlers[type]))();
  }

  void update() {
    if (!didInit) {
      initPin(pin);
      didInit = true;
    }
    bool buttonPressed = isButtonPressed();
    if (ignoreEventsUntilFirstButtonUp && buttonPressed && !seenFirstButtonUp) {
      // button pressed on launch, ignore events until it is released after launch
      if (!loggedSeenFirstButtonUp) {
        logf("Ignoring button events until first button-up...");
        loggedSeenFirstButtonUp = true;
      }
      return;
    }
    seenFirstButtonUp = true;
    long readTime = millis();

    if (!buttonPressed && buttonDownTime != 0) {
      doHandler(buttonUp);
    }

    if (waitForButtonUpLongPress || waitForButtonUpVeryLongPress)  {
      if (!waitForButtonUpVeryLongPress && buttonPressed && readTime - buttonDownTime > veryLongPressInterval) {
        doHandler(veryLongPress);
        waitForButtonUpVeryLongPress = true;
      }
      if (!buttonPressed) {
        waitForButtonUpLongPress = false;
        waitForButtonUpVeryLongPress = false;
      }
    } else {
      if (!buttonPressed && singlePressTime != 0) {
        if ((!handlers[doublePress] && !handlers[doubleLongPress]) || readTime - singlePressTime > doublePressInterval) {
          // double-press timeout
          doHandler(singlePress);
          singlePressTime = 0;
        }
      }
      if (!buttonPressed && buttonDownTime != 0) {
        if (singlePressTime != 0) {
          // button-up from second press
          doHandler(doublePress);
          singlePressTime = 0;
        } else {
          singlePressTime = readTime;
        }
      } else if (buttonPressed && buttonDownTime == 0) {
        if (readTime - singlePressTime < 15) {
          // The metal buttons sometimes have state jitter leading to a button down around ~10ms after button up. We'll ignore.
          logf("Button jitter detected at %lums, ignoring", readTime - singlePressTime);
          return;
        }
        buttonDownTime = readTime;
        doHandler(buttonDown);
      } else if (buttonPressed && readTime - buttonDownTime > longPressInterval) {
        if (singlePressTime != 0) {
          doHandler(doubleLongPress);
          singlePressTime = 0;
        } else {
          doHandler(longPress);
        }
        waitForButtonUpLongPress = true;
      }
    }

    if (!buttonPressed) {
      buttonDownTime = 0;
    }
  }

public:
  uint16_t longPressInterval = 500;
  uint16_t veryLongPressInterval = 6666;
  uint16_t doublePressInterval = 400;
  bool pressedState = LOW;

  bool ignoreEventsUntilFirstButtonUp = false;

  SPSTButton(int pin) : HardwareControl(pin) { }
  ~SPSTButton() {
    for (int i = 0; i < handlerTypeCount; ++i) {
      if (handlers[i]) {
        delete handlers[i];
      }
    }
  }

  virtual bool isButtonPressed() {
    return digitalRead(pin) == pressedState;
  }

  // guestural handlers that handle all timeouts/delays, i.e. singlePress handler will only be called if doublePress is not triggered.
  void onSinglePress(ButtonHandler handler) {
    onHandler(singlePress, handler);
  }
 
  void onDoublePress(ButtonHandler handler) {
    onHandler(doublePress, handler);
  }

  void onLongPress(ButtonHandler handler) {
    onHandler(longPress, handler);
  }

  void onVeryLongPress(ButtonHandler handler) {
    onHandler(veryLongPress, handler);
  }

  void onDoubleLongPress(ButtonHandler handler) {
    onHandler(doubleLongPress, handler);
  }

  // simple handlers. onButtonUp may be called in addition to the e.g. longPress handler if both are set. onButtonUp will be called twice during doublePress.
  void onButtonDown(ButtonHandler handler) {
    onHandler(buttonDown, handler);
  }

  void onButtonUp(ButtonHandler handler) {
    onHandler(buttonUp, handler);
  }
};

/* --------------------- */

class HardwareControls {
private:
  std::vector<HardwareControl *> controlsVec;
public:
  ~HardwareControls() {
    for (auto control : controlsVec) {
      delete control;
    }
    controlsVec.clear();
  }

  HardwareControl *addControl(HardwareControl *control) {
    controlsVec.push_back(control);
    return control;
  }

  SPSTButton *addButton(int pin, bool pressedState=LOW) {
    SPSTButton *button = new SPSTButton(pin);
    button->pressedState = pressedState;
    controlsVec.push_back(button);
    return button;
  }

  AnalogDial *addAnalogDial(int pin) {
    AnalogDial *dial = new AnalogDial(pin);
    controlsVec.push_back(dial);
    return dial;
  }

  void update() {
    for (HardwareControl *control : controlsVec) {
      control->update();
    }
  }
};

#endif
