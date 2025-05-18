#ifndef PATTERNMANAGER_H
#define PATTERNMANAGER_H

#include <vector>
#include <list>
#include <functional>
#include <algorithm>
#include <drawing.h>
#include <paletting.h>

using ColorManager = PaletteRotation<CRGBPalette256>;
#if DUSTLIB_SHARED_COLORMANAGER
ColorManager sharedColorManager;
#endif

// FIXME: I dislike this. 
// but the alternative is to either require Pattern<SIZE>, Pattern(SIZE), or a custom pixel storage which doesn't inherit from CPixelView.
// and I'm not sure I like those options any better.
// we already need LED_COUNT for mapping.h already, so maybe this is fine.
#ifndef LED_COUNT
#error "patterning.h needs LED_COUNT defined"
#endif
using DrawingContext = PixelStorage<LED_COUNT>;

class Composable {
private:
  uint8_t targetAlpha = 0xFF;
  uint8_t animationSpeed = 1;
public:
  uint8_t alpha = 0xFF;
  uint8_t maxAlpha = 0xFF; // convenience, scales all brightness values by this amount
  virtual ~Composable() { }

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
    if (alpha > 0) {
      this->ctx.blendIntoContext(otherContext, BlendMode::blendBrighten, scale8(alpha, maxAlpha));
    }
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

  virtual void setup() { }

  void stop() {
    logf("Stopping %s", description());
    startTime = -1;
    stopTime =  millis();
  }

  virtual void update() { }
  
  virtual const char *description() = 0;

public:
  inline bool isRunning() {
    return startTime != -1;
  }

  inline bool isStopped() {
    // different from !isRunning
    return stopTime != -1;
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

class PatternRunner;

// function for creating a Pattern instance for a given PatternRunner
using PRConstructor = std::function<Pattern*(PatternRunner&)>;
// function for testing a Pattern-conditional predicate, return pattern alpha as uint8_t, 0 to stop
using PRPredicate = std::function<uint8_t(PatternRunner&)>;
// function for informing the caller of an event
using PRCompletion = std::function<void(PatternRunner&)>;

class PatternRunner {
public:
  Pattern *pattern = NULL;
  PRConstructor constructor;
  uint8_t priority = 0;  // highest priority dims background patterns by dimAmount
  uint8_t dimAmount = 0; // if highest priority, dim other patterns by this amount
  bool paused = false;
  bool complete = false; // true if the runner's task is complete and the runner itself can be removed

  PatternRunner(PRConstructor constructor) : constructor(constructor) { }

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

  virtual void setAlpha(uint8_t alpha, bool animated=true) {
    if (pattern) {
      pattern->setAlpha(alpha, animated, 10);
    }
  }

  virtual void loop() {
    if (pattern && pattern->isRunning() && !paused) {
      pattern->loop();
    }
  }

  virtual void draw(DrawingContext ctx) {
    if (pattern && pattern->isRunning() && !paused) {
      pattern->composeIntoContext(ctx);
    }
  }
};

class OneShotPatternRunner : public PatternRunner {
  PRCompletion completion;
public:
  OneShotPatternRunner(PRConstructor constructor) : PatternRunner(constructor) { }
  virtual void stop() {
    PatternRunner::stop();
    complete = true;
    if (completion) {
      completion(*this);
    }
  }
  virtual void loop() {
    PatternRunner::loop();
    if (pattern && pattern->isStopped()) {
      stop();
    }
  }
};

class ConditionalPatternRunner : public PatternRunner {
  PRPredicate runCondition; // 0 to stop, >0 to run at returned alpha
public:
  ConditionalPatternRunner( PRConstructor constructor, 
                            PRPredicate runCondition
                          ) : PatternRunner(constructor), runCondition(runCondition) { }
  
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
      } else if (pattern) {
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

// TODO: would be nice for this to subclass IndexedPatternRunner and get manual pattern change by index for free
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
  CrossfadingPatternRunner(PRConstructor constructor) : PatternRunner(constructor) { }

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

class PatternManager {
  // pattern lifecycle management
  std::vector<PatternRunner *> runners;
  PatternRunner* testRunner = NULL; // used by setTestPattern for an exclusive dev mode

  std::vector<Pattern * (*)(void)> patternConstructors; // for indexed and random pattern modes

  DrawingContext &ctx;

  template<class T>
  static Pattern *construct() {
    Derived_from<T, Pattern>();
    return new T();
  }
public:
  PatternManager(DrawingContext &ctx) : ctx(ctx) { }

  ~PatternManager() {
    for (auto runner : runners) {
      delete runner;
    }
  }

  // Test pattern runs by default and in exclusive mode
  template<class T>
  PatternRunner& setTestRunner() {
    PatternRunner *runner = new ConditionalPatternRunner([](PatternRunner& runner) {
      return construct<T>();
    }, [](PatternRunner&) {
      return 0xFF; // always run
    });
    runner->priority = 0xFF;
    runner->dimAmount = 0xFF;
    testRunner = runner;
    return *runner;
  }
  
  // Add a pattern class to the patterns list for the random and indexed runners to use
  template<class T>
  inline void registerPattern() {
    patternConstructors.push_back(&(construct<T>));
  }

  // Creates pattern with constructor immediately runs it. Destroyed once the pattern is stopped. Dims other patterns by dimAmount if highest priority.
  // FIXME: can this return a nulling-pointer? 
  OneShotPatternRunner *runOneShotPattern(PRConstructor constructor, uint8_t priority=0, uint8_t dimAmount=0, PRCompletion completion=nullptr) {
    OneShotPatternRunner *runner = new OneShotPatternRunner(constructor);
    runner->dimAmount = dimAmount;
    runner->priority = priority;
    runners.push_back(runner);
    runner->start();
    return runner;
  }

  // When runCondition > 0, creates pattern with constructor and fades it in using runCondition return value. Dims other patterns by dimAmount if highest priority.
  ConditionalPatternRunner* setupConditionalRunner(PRConstructor constructor, PRPredicate runCondition, uint8_t priority=0, uint8_t dimAmount=0) {
    ConditionalPatternRunner *runner = new ConditionalPatternRunner(constructor, runCondition);
    runner->priority = priority;
    runner->dimAmount = dimAmount;
    runners.push_back(runner);
    return runner;
  }

  // When runCondition > 0, creates given pattern class and fades it in using runCondition return value. Dims other patterns by dimAmount if highest priority.
  template<class T>
  inline ConditionalPatternRunner* setupConditionalRunner(PRPredicate runCondition, uint8_t priority=0, uint8_t dimAmount=0) {
    return setupConditionalPattern([](PatternRunner&) { return construct<T>(); }, runCondition, priority, dimAmount);
  }

  // Start a random pattern from the patterns list with optional crossfade
  CrossfadingPatternRunner* setupRandomRunner(unsigned long runDuration=40*1000, unsigned long crossfadeDuration=500) {
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
  IndexedPatternRunner *setupIndexedRunner(unsigned int index=0) {
    IndexedPatternRunner *runner = new IndexedPatternRunner(patternConstructors);
    runner->patternIndex = index;
    runners.push_back(runner);
    return runner;
  }

  void removeAllRunners() {
    for (auto runner : runners) {
      delete runner;
    }
    runners.clear();
  }

  void removeRunner(PatternRunner* runner) {
    auto it = std::find(runners.begin(), runners.end(), runner);
    if(it != runners.end()) {
      (*it)->stop();
      delete *it;
      runners.erase(it);
    } else {
      assert(false, "Attempt to remove a runner that was not found");
    }
  }
public:
  // Returns a priority higher than what's currently running
  uint8_t highestPriority() {
    uint8_t maxPriority = 0;
    for (PatternRunner *runner : runners) {
      if (runner->priority > maxPriority) {
        maxPriority = runner->priority;
      }
    }
    assert(maxPriority < 0xFF, "already at max priority");
    return min(0xFF, maxPriority + 1);
  }

  void setup() { }

  void loop() {
    ctx.leds.fill_solid(CRGB::Black);
    
    uint8_t maxPriority = 0;
    uint8_t priorityDimAmount = 0;
    if (!testRunner) {
      for (PatternRunner *runner : runners) {
        runner->loop();
        if (runner->priority > maxPriority && runner->pattern && !runner->paused) {
          // simplified: only considers dimming from the max priority runner.
          maxPriority = runner->priority;
          priorityDimAmount = runner->dimAmount;
        }
      }
      for (PatternRunner *runner : runners) {
        runner->setAlpha(0xFF - (runner->priority < maxPriority ? priorityDimAmount : 0));
        runner->draw(ctx);
      }
    } else {
      // special-case testRunner so that no other patterns are ever run
      testRunner->loop();
      testRunner->setAlpha(0xFF);
      testRunner->draw(ctx);
    }
    for (std::vector<PatternRunner *>::iterator it = runners.begin(); it < runners.end(); ) {
      if ((*it)->complete) {
        logdf("Removing a complete runner");
        delete *it;
        runners.erase(it);
      } else {
        ++it;
      }
    }
  }
};

#endif
