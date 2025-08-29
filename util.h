#ifndef UTIL_H
#define UTIL_H

#include <Arduino.h>
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */
#include <functional>
#include <FastLED.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define ARRAY_SAMPLE(a) (ARRAY_SIZE(a) < 255 ? a[random8(ARRAY_SIZE(a))] : a[random16(ARRAY_SIZE(a))])

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

// used to enforce template base class
template<class T, class B> struct Derived_from {
  static void constraints(T* p) { B* pb = p; (void)pb; }
  Derived_from() { void(*p)(T*) = constraints; (void)p; }
};

int freeRAM();

#if DEBUG
#define logdf(format, ...) logf(format, ## __VA_ARGS__)
#else
#define logdf(format, ...)
#endif

static int vasprintf(char** strp, const char* fmt, va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  char tmp[1];
  int size = vsnprintf(tmp, 1, fmt, ap2);
  if (size <= 0) {
    *strp=NULL;
    return size;
  }
  va_end(ap2);
  size += 1;
  *strp = (char*)malloc(size * sizeof(char));
  return vsnprintf(*strp, size, fmt, ap);
}

static void _logf(bool newline, const char *format, va_list argptr)
{
  if (!Serial) return;
  if (strlen(format) == 0) {
    if (newline) {
      Serial.println();
    }
    return;
  }
  char *buf;
  vasprintf(&buf, format, argptr);
  if (newline) {
    Serial.println(buf ? buf : "LOGF MEMORY ERROR");
  } else {
    Serial.print(buf ? buf : "LOGF MEMORY ERROR");
  }
#if DEBUG
  Serial.flush();
#endif
  free(buf);
}

void logf(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  _logf(true, format, argptr);
  va_end(argptr);
}

void loglf(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  _logf(false, format, argptr);
  va_end(argptr);
}

#undef assert
#define assert(expr, reasonFormat, ...) assert_func((expr), #expr, reasonFormat, ## __VA_ARGS__);

void assert_func(bool result, const char *pred, const char *reasonFormat, ...) {
  if (!result) {
    logf("ASSERTION FAILED: %s", pred);
    va_list argptr;
    va_start(argptr, reasonFormat);
    _logf(true, reasonFormat, argptr);
    va_end(argptr);
#if DEBUG
    while (1) delay(100);
#endif
  }
}

#define TIMEIT(name, ...) \
  auto __start##name = micros(); \
  __VA_ARGS__; \
  auto __end##name = micros(); \
  logf("%s took %ius", #name, (__end##name - __start##name));

template <typename T>
inline int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

template<typename T>
inline T mod_distance(T a, T b, T m) {
  return (m / 2 - ((3 * m) / 2 + a - b) % m);
}
#define MOD_DISTANCE(a, b, m) (m / 2. - fmod((3 * m) / 2 + a - b, m))

inline int mod_wrap(int x, int m) {
  int result = x % m;
  return result < 0 ? result + m : result;
}

inline float fmod_wrap(float x, int m) {
  float result = fmod(x, m);
  return result < 0 ? result + m : result;
}

void DrawModal(int fps, unsigned long durationMillis, std::function<void(unsigned long elapsed)> tick) {
  int delayMillis = 1000/fps;
  unsigned long start = millis();
  unsigned long elapsed = 0;
  do {
    unsigned long startMils = millis();
    tick(elapsed);
    FastLED.show();
    FastLED.delay(max(0, delayMillis - (millis()-startMils)));
    elapsed = millis() - start;
  } while (elapsed < durationMillis);
}

class FrameCounter {
  private:
    unsigned long lastPrint = 0;
    long frames = 0;
    long lastClamp = 0;
  public:
    long printInterval = 2000;
    int framerateLogging = true;

    int fps() {
      unsigned long mil = millis();
      long elapsed = MAX(1, mil - lastPrint);
      return (int)(frames / (float)elapsed * 1000);
    }

    void loop() {
      unsigned long mil = millis();
      long elapsed = MAX(1, mil - lastPrint);
      if (framerateLogging && elapsed > printInterval) {
        if (lastPrint != 0) {
          // arduino-samd-core can't sprintf floats??
          // not sure why it's not working for me, I should have Arduino SAMD core v1.8.9
          // https://github.com/arduino/ArduinoCore-samd/issues/407
          logf("Framerate: %i, free mem: %i", fps(), freeRAM());
        }
        frames = 0;
        lastPrint = mil;
      }
      ++frames;
    }
    void clampToFramerate(int fps) {
      if (fps > 0) {
        int delayms = 1000 / fps - (millis() - lastClamp);
        if (delayms > 0) {
#ifdef FASTLED_VERSION
          FastLED.delay(delayms);
#else
          delay(delayms);
#endif
        }
        lastClamp = millis();
      }
    }
};

class PhotoSensorBrightness {
  bool firstLoop = true;

public:
  const int readPin;
  const int powerPin;
  bool flipSensor = false;
  uint8_t minBrightness = 2;
  uint8_t maxBrightness = 0xFF;
  uint8_t threshold = 2; // minimum brightness change, to reduce brightness flicker
  bool logChanges = false;
  bool paused = false;

  PhotoSensorBrightness(int readPin, int powerPin=-1) : readPin(readPin), powerPin(powerPin) { }

  void setPower(bool power) {
    digitalWrite(powerPin, power);
  }

  void loop() {
    if (firstLoop) {
      pinMode(readPin, INPUT);
      if (powerPin != -1) {
        pinMode(powerPin, OUTPUT);
      }
      firstLoop = false;
    }
    if (paused) return;
    if (powerPin != -1) {
      setPower(true);
    }
    int photoRead = analogRead(readPin);
    // TODO: leaving photosensor power pin on because otherwise results are inconsistent
    if (powerPin != -1) {
      // setPower(false);
    }
    uint8_t targetBrightness = 0xFF * min(0x400, photoRead) / 0x400;
    if (flipSensor) {
      targetBrightness = 0xFF - targetBrightness;
    }
    targetBrightness = max(minBrightness, scale8(targetBrightness, maxBrightness));
    uint8_t currentBrightness = FastLED.getBrightness();
    int diff = targetBrightness - currentBrightness;
    if (abs(diff) > threshold) {
      uint8_t nextBrightness = currentBrightness + (diff < 0 ? -1 : 1);
      if (logChanges) {
        logf("currentBrightness=%i, targetBrightness=%i, setBrightness->%i", currentBrightness, targetBrightness, nextBrightness);
      }
      FastLED.setBrightness(nextBrightness);
    }
  }
};

struct DebounceDigital {
private:
  int stableRead;
  int lastRead;
  unsigned long lastChange;
public:
  unsigned long stableMicros;
  // debounce returns startingValue until value inititally stabilizes for stableMicros duration
  DebounceDigital(int startingValue) : stableRead(startingValue), lastRead(startingValue), lastChange(0), stableMicros(10000) { }
  // returns first value read and begins stabilization from that read
  DebounceDigital() : stableRead(-1), lastRead(-1), lastChange(0), stableMicros(10000) { }
  // idk why the default copy constructor isn't working?
  DebounceDigital(DebounceDigital &other) : stableRead(other.stableRead), lastRead(other.lastRead), lastChange(other.lastChange), stableMicros(other.stableMicros) { }
  int debounce(PinStatus read) {
    if (read != lastRead) {
      lastChange = micros();
      lastRead = read;
    }
    if (lastChange == 0 || stableRead == -1 || micros() - lastChange > stableMicros) {
      stableRead = read;
    }
    return stableRead;
  }

  int digitalRead(pin_size_t pin) {
    return debounce(::digitalRead(pin));
  }
};

uint8_t sawtoothEvery(unsigned long repeatEveryMillis, unsigned riseTime, int phase=0) {
    unsigned long sawtooth = (millis() + phase) % repeatEveryMillis;
    if (sawtooth > repeatEveryMillis-riseTime) {
      return 0xFF * (sawtooth-repeatEveryMillis+riseTime) / riseTime;
    } else if (sawtooth < riseTime) {
      return 0xFF - 0xFF * sawtooth / riseTime;
    }
    return 0;
}

template <typename T, uint16_t SIZE>
void shuffle(T arr[SIZE]) {
  for (uint16_t i = 0; i < SIZE; ++i) {
    uint16_t swap = random16(i, SIZE);
    T tmp;
    tmp = arr[i];
    arr[i] = arr[swap];
    arr[swap] = tmp;
  }
}

int lsb_noise(int pin, int numbits) {
  // pulling the pin up briefly prevents it from converging to a value on repeated calls to lsb_noise
  pinMode(pin, INPUT_PULLUP);
  pinMode(pin, INPUT);
  
  int noise = 0;
  int lastVal = 0;
  for (int i = 0; i < numbits; ++i) {
    int val = analogRead(pin);
    if (val != lastVal) { // repeated reads on a floating pin tend to converge, so we'll skip them when they start doing this.
      noise = (noise << 1) | (val & 1);
      lastVal = val;
    }
  }
  return noise;
}

void printColor(CRGB color) {
  loglf("CRGB(0x%x, 0x%x, 0x%x)", color.r, color.g, color.b);
}

void printColor(CHSV color) {
  loglf("CHSV(0x%x, 0x%x, 0x%x)", color.h, color.s, color.v);
}

extern "C" char* sbrk(int incr);
int freeRAM() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif
}

#endif
