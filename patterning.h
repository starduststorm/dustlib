#ifndef PATTERNMANAGER_H
#define PATTERNMANAGER_H

#include <vector>
#include <list>

#include "drawing.h"
#include "ledgraph.h" // FIXME: gotta get LED_COUNT out of the library without having to put it on every Pattern
#include <paletting.h>

using ColorManager = PaletteRotation<CRGBPalette256>;
#if DUSTLIB_SHARED_COLORMANAGER
ColorManager sharedColorManager;
#endif

// used to enforce template base class
template<class T, class B> struct Derived_from {
  static void constraints(T* p) { B* pb = p; (void)pb; }
  Derived_from() { void(*p)(T*) = constraints; }
};

class Composable {
private:
  uint8_t targetAlpha = 0xFF;
  uint8_t animationSpeed = 1;
public:
  uint8_t alpha = 0xFF;
  uint8_t maxAlpha = 0xFF; // convenience, scales all brightness values by this amount
  virtual ~Composable() { }
  // FIXME: needs to use the drawing context type that PatternManager uses
  DrawingContext ctx;
  
  void setAlpha(uint8_t b, bool animated=false, uint8_t speed=1) {
    targetAlpha = b;
    animationSpeed = speed;
    if (!animated) {
      alpha = b;
    }
  }

  void composeIntoContext(DrawingContext &otherContext) {
    if (alpha != targetAlpha) {
      if (abs(targetAlpha - alpha) < animationSpeed) {
        alpha = targetAlpha;
      } else {
        alpha += animationSpeed * sgn((int)targetAlpha - (int)alpha);
      }
    }
    this->ctx.blendIntoContext(otherContext, BlendMode::blendBrighten, scale8(alpha, maxAlpha));
  }
};

class Pattern : public Composable {
private:  
  long startTime = -1;
  long stopTime = -1;
  long lastUpdateTime = -1;
public:
  virtual ~Pattern() { }

  void start() {
    logf("Starting %s", description());
    startTime = millis();
    stopTime = -1;
    setup();
  }

  void loop() {
    if (alpha > 0) {
      update();
    }
    lastUpdateTime = millis();
  }

  virtual bool wantsToIdleStop() {
    return true;
  }

  virtual void setup() { }

  void stop() {
    logf("Stopping %s", description());
    startTime = -1;
  }

  virtual void update() { }
  
  virtual const char *description() = 0;

public:
  bool isRunning() {
    return startTime != -1;
  }

  unsigned long runTime() {
    return startTime == -1 ? 0 : millis() - startTime;
  }

  unsigned long frameTime() {
    return (lastUpdateTime == -1 ? 0 : millis() - lastUpdateTime);
  }
};

class BlankPattern : public Pattern {
public:
  BlankPattern() { }
  void update() { }
  const char *description() {
    return "BlankPattern";
  }
};

/* ================================================================================ */

class PatternRunner {
public:
  Pattern *pattern = NULL;
  function<Pattern*(PatternRunner&)> constructor;
  uint8_t priority = 0; // higher priority dims background patterns by dimAmount
  uint8_t dimAmount = 0;
  bool paused = false;

  PatternRunner(function<Pattern*(PatternRunner&)> constructor) : constructor(constructor) { }

  virtual ~PatternRunner() {
    delete pattern;
  }

  virtual bool start() {
    assert(pattern == NULL, "attempt to run a pattern that's already running");
    pattern = constructor(*this);
    pattern->start();
    return true;
  }

  virtual void stop() {
    if (pattern) {
      pattern->stop();
      delete pattern;
      pattern = NULL;
    }
  }

  virtual void setAlpha(uint8_t alpha) {
    if (pattern) {
      pattern->setAlpha(alpha, true, 4);
    }
  }

  virtual void loop() {
    if (pattern && !paused) {
      pattern->loop();
    }
  }

  virtual void draw(DrawingContext ctx) {
    if (pattern && !paused) {
      pattern->composeIntoContext(ctx);
    }
  }
};

class ConditionalPatternRunner : public PatternRunner {
  function<uint8_t(PatternRunner&)> runCondition; // 0 to stop, >0 to run at returned alpha
public:
  ConditionalPatternRunner(function<Pattern*(PatternRunner&)> constructor, function<uint8_t(PatternRunner&)> runCondition) 
    : PatternRunner(constructor), runCondition(runCondition) { }
  
  virtual void loop() {
    if (!paused) {
      uint8_t runFade = runCondition(*this);
      if (runFade > 0) {
        if (!pattern) {
          start();
        }
        if (pattern) {
          pattern->maxAlpha = runFade;
        }
      } else {
        stop();
      }
    }
    PatternRunner::loop();
  }
};

class IndexedPatternRunner : public PatternRunner {
public:
  unsigned int patternIndex = 0;
  std::vector<Pattern * (*)(void)>& patternConstructors;
  IndexedPatternRunner(std::vector<Pattern * (*)(void)>& patternConstructors) : PatternRunner([this](PatternRunner& runner) {
      return this->patternConstructors[this->patternIndex]();
    }), patternConstructors(patternConstructors) {}

  void runPatternAtIndex(unsigned int index) {
    if (index < patternConstructors.size()) {
      stop();
      patternIndex = index;
      start();
    }
  }
  
  void nextPattern() {
    patternIndex = (patternIndex + 1) % patternConstructors.size();
    runPatternAtIndex(patternIndex);
  }

  void previousPattern() {
    patternIndex = mod_wrap(patternIndex - 1, patternConstructors.size());
    runPatternAtIndex(patternIndex);
  }

  virtual void loop() {
    if (!pattern) {
      start();
    }
    PatternRunner::loop();
  }
};

class CrossfadingPatternRunner : public PatternRunner {
  Pattern *crossfadePattern = NULL;
  uint8_t crossfadeAlpha(Pattern *p) {
    if (p) {
      uint8_t fadeIn = constrain(0xFF * p->runTime() / crossfadeDuration, 0, 0xFF);
      uint8_t fadeOut = patternTimeout == 0 ? 0xFF :
                       constrain(0xFF * (patternTimeout - p->runTime()) / crossfadeDuration, 0, 0xFF);
      return dim8_raw(min(fadeIn, fadeOut));
    }
    return 0;
  }
public:
  unsigned long patternTimeout = 40*1000;
  unsigned long crossfadeDuration = 500;
  int context=-1; // hackyhack used to not repeat random choice
  CrossfadingPatternRunner(function<Pattern*(PatternRunner&)> constructor) : PatternRunner(constructor) { }

  virtual void setAlpha(uint8_t alpha) {
    PatternRunner::setAlpha(alpha);
    if (crossfadePattern) {
      crossfadePattern->setAlpha(alpha, true, 4);
    }
  }

  virtual void loop() {
    assert(pattern || !crossfadePattern, "inconsistent crossfade state");
    if (!pattern) {
      start();
      pattern->alpha = 0;
    }
    if (pattern && !paused) {
      if (pattern->runTime() > patternTimeout) {
        // done crossfading
        stop();
        pattern = crossfadePattern;
        crossfadePattern = NULL;
      } else if (!crossfadePattern && pattern->runTime() > patternTimeout - crossfadeDuration) {
        // start crossfade
        crossfadePattern = constructor(*this);
        crossfadePattern->alpha = 0;
        crossfadePattern->start();
      }
      pattern->maxAlpha = crossfadeAlpha(pattern);
    }
    if (crossfadePattern && !paused) {
      crossfadePattern->maxAlpha = crossfadeAlpha(crossfadePattern);
      crossfadePattern->loop();
    }
    PatternRunner::loop();
  }
  
  virtual void draw(DrawingContext ctx) {
    if (crossfadePattern && !paused) {
      crossfadePattern->composeIntoContext(ctx);
    }
    PatternRunner::draw(ctx);
  }
};

/* ================================================================================ */

template <typename DrawingContextType>
class PatternManager {
  vector<PatternRunner *> runners;                      // pattern lifecycle management
  std::vector<Pattern * (*)(void)> patternConstructors; // for indexed and random pattern modes

  DrawingContextType &ctx;

  template<class T>
  static Pattern *construct() {
    Derived_from<T, Pattern>();
    return new T();
  }
public:
  PatternManager(DrawingContextType &ctx) : ctx(ctx) { }

  ~PatternManager() {
    for (auto runner : runners) {
      delete runner;
    }
  }

  // Test pattern runs by default and in exclusive mode
  template<class T>
  PatternRunner& setTestPattern() {
    PatternRunner *runner = new ConditionalPatternRunner([](PatternRunner& runner) {
      logf("setTestPattern construct");
      return construct<T>();
    }, [](PatternRunner&) {
      return 0xFF; // always run
    });
    runner->priority = 0xFF;
    runner->dimAmount = 0xFF;
    runners.push_back(runner);
    return *runner;
  }
  
  // Add a pattern class to the patterns list
  template<class T>
  inline void registerPattern() {
    patternConstructors.push_back(&(construct<T>));
  }

  // Add a pattern class to run conditionally
  ConditionalPatternRunner* setupConditionalPattern(function<Pattern*(PatternRunner&)> constructor, function<uint8_t(PatternRunner&)> runCondition, uint8_t priority=0, uint8_t dimAmount=0) {
    ConditionalPatternRunner *runner = new ConditionalPatternRunner(constructor, runCondition);
    runner->priority = priority;
    runner->dimAmount = dimAmount;
    runners.push_back(runner);
    return runner;
  }

  template<class T>
  inline ConditionalPatternRunner* setupConditionalPattern(function<uint8_t(PatternRunner&)> runCondition, uint8_t priority=0, uint8_t dimAmount=0) {
    return setupConditionalPattern([](PatternRunner&) { return construct<T>(); }, runCondition, priority, dimAmount);
  }

  // Start a random pattern from the patterns list with optional crossfade
  CrossfadingPatternRunner* setupRandomPattern(unsigned long runDuration=40*1000, unsigned long crossfadeDuration=500) {
    CrossfadingPatternRunner *runner = new CrossfadingPatternRunner([this](PatternRunner& runner) {
      CrossfadingPatternRunner &xr = (CrossfadingPatternRunner&)runner; // whee bc we know
      int choice;
      do {
        choice = random16(this->patternConstructors.size());
      } while (choice == xr.context && this->patternConstructors.size() > 1);      
      logdf("setupRandomPattern chose %i", choice);
      xr.context = choice; // hackyhack save off choice to not repeat it. it'd be nice to find a better way.
      Pattern *pattern = this->patternConstructors[choice]();
      return pattern;
    });
    runner->patternTimeout = runDuration;
    runner->crossfadeDuration = crossfadeDuration;
    runners.push_back(runner);
    return runner;
  }

  // Start a pattern by list index
  IndexedPatternRunner *setupIndexedPattern(unsigned int index=0) {
    IndexedPatternRunner *runner = new IndexedPatternRunner(patternConstructors);
    runner->patternIndex = index;
    runners.push_back(runner);
    return runner;
  }

  void removeRunner(PatternRunner* runner) {
    auto it = std::find(runners.begin(), runners.end(), runner);
    if(it != runners.end()) {
      (*it)->stop();
      delete *it;
      runners.erase(it);
    } else {
      logf("Attempt to remove a runner that was not found");
    }
  }
public:
  void setup() { }

  void loop() {
    ctx.leds.fill_solid(CRGB::Black);
    
    uint8_t maxPriority = 0;
    uint8_t priorityDimAmount = 0;
    for (PatternRunner *runner : runners) {
      runner->loop();
      if (runner->priority > maxPriority) {
        // simplified: only considers dimming from the max priority runner.
        maxPriority = runner->priority;
        priorityDimAmount = runner->dimAmount;
      }
    }
    for (PatternRunner *runner : runners) {
      // FIXME: this animates even when a test pattern is running, which makes the test pattern non-exclusive
      runner->setAlpha(0xFF - (runner->priority < maxPriority ? priorityDimAmount : 0));
      runner->draw(ctx);
    }
  }
};

#endif
