#ifndef PATTERNMANAGER_H
#define PATTERNMANAGER_H

#include <memory>
#include <vector>
#include <list>
#include <map>
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
  bool firstAlphaSet = false;
public:
  uint8_t alpha = 0xFF;
  uint8_t maxAlpha = 0xFF; // convenience, scales all brightness values by this amount
  virtual ~Composable() { }

  DrawingContext ctx;
  
  void setAlpha(uint8_t b, bool animated=false, uint8_t speed=1) {
    if (!firstAlphaSet) {
      // decline to animate the first alpha set on brand new composables so that we start from whatever the baseline is
      animated = false;
      firstAlphaSet = true;
    }
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
  bool updateWhileHidden = false; // set to true to continue to run pattern update even while pattern is not being drawn
  virtual ~Pattern() { }

  void start() {
    logf("Starting %s", description());
    startTime = millis();
    stopTime = -1;
    setup();
  }

  void loop() {
    if (updateWhileHidden || alpha > 0) {
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
class ConditionalPatternRunner;
class CrossfadingPatternRunner;
class IndexedPatternRunner;

// function for creating a Pattern instance for a given PatternRunner
using PRConstructor = std::function<Pattern*(PatternRunner&)>;
// function for testing a Pattern-conditional predicate, return pattern alpha as uint8_t, 0 to stop
using PRPredicate = std::function<uint8_t(PatternRunner&)>;
// function for informing the caller of an event
using PRCompletion = std::function<void(PatternRunner&)>;

/* ================================================================================ */

class PatternManager {
  friend IndexedPatternRunner;
  // pattern lifecycle management
  std::vector<std::shared_ptr<PatternRunner> > runners;

  // used by setTestPattern for an exclusive dev mode
  PatternRunner* testRunner = NULL; 

  // constructors for all registered patterns
  std::vector<Pattern * (*)(void)> patternConstructors; 

  // pattern groups (map:group index->constructor index) for indexed and random runners (default group 0)
  std::map<int, std::vector<int> > patternGroupMap;

  DrawingContext &ctx;

  template<class T>
  static Pattern *construct() {
    Derived_from<T, Pattern>();
    return new T();
  }

public:

  std::shared_ptr<PatternRunner> addRunner(PatternRunner *runner);
  void removeRunner(std::shared_ptr<PatternRunner> runner);
  void removeAllRunners();

  PatternManager(DrawingContext &ctx);

  // Test pattern runs by default and in exclusive mode
  template<class T>
  PatternRunner& setTestRunner();
  bool hasTestRunner();
  
  // Add a pattern class to the patterns group list for the random and indexed runners to use. Returns pattern index.
  template<class T>
  unsigned int registerPattern(int groupID=0);

  // Patterns can be registered to a group index so that multiple indexed lists can be maintained. Groups can be ignored to operate on group 0.
  unsigned int groupAddPatternIndex(unsigned int patternIndex, int groupID);
  void groupRemovePatternIndex(unsigned int patternIndex, int groupID);
  std::vector<int> patternIndexesInGroup(int groupID);
  Pattern *createPattern(unsigned int patternIndex, int groupID);
  bool isValidGroupIndex(unsigned int patternIndex, int groupID);

  // Creates a random pattern selected from the given groupID and immediately runs it; destroys the pattern once it's stopped. Dims other patterns by dimAmount if highest priority.
  // Only intended to be used with patterns that end on their own.
  std::shared_ptr<PatternRunner> runRandomOneShotFromGroup(int groupID, uint8_t priority, uint8_t dimAmount);

  // Creates pattern with constructor immediately runs it; destroys the pattern once it's stopped. Dims other patterns by dimAmount if highest priority.
  // Only intended to be used with patterns that end on their own.
  std::shared_ptr<PatternRunner> runOneShotPattern(PRConstructor constructor, uint8_t priority=0, uint8_t dimAmount=0, PRCompletion completion=nullptr);

  // Creates pattern of given type and immediately runs it; destroys the pattern once it's stopped. Dims other patterns by dimAmount if highest priority.
  // Only intended to be used with patterns that end on their own.
  template<class T>
  std::shared_ptr<PatternRunner> runOneShotPattern(uint8_t priority, uint8_t dimAmount, PRCompletion completion=nullptr);

  // When runCondition > 0, creates pattern with constructor and fades it in using runCondition return value. Dims other patterns by dimAmount if highest priority.
  ConditionalPatternRunner* setupConditionalRunner(PRConstructor constructor, PRPredicate runCondition, uint8_t priority=0, uint8_t dimAmount=0);

  // When runCondition > 0, creates given pattern class and fades it in using runCondition return value. Dims other patterns by dimAmount if highest priority.
  template<class T>
  inline ConditionalPatternRunner* setupConditionalRunner(PRPredicate runCondition, uint8_t priority=0, uint8_t dimAmount=0);

  // Run random patterns from the patterns list automatically, crossfading between them
  CrossfadingPatternRunner* setupRandomRunner(unsigned long runDuration=40*1000, unsigned long crossfadeDuration=500, int groupID=0);

  // Run pattern by list index with no auto advancement
  IndexedPatternRunner *setupIndexedRunner(unsigned int startIndex, int groupID=0);

  // Run pattern by list index, crossfading patter changes, with no auto advancement with default patternTimeout=0
  CrossfadingPatternRunner *setupCrossfadingRunner(unsigned int startIndex, int groupID=0);

  // Returns a priority higher than what's currently running, or 0xFF
  uint8_t highestPriority();

  void setup();
  void loop();
};

/* ================================================================================ */

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
    if (pattern) {
      pattern->start();
    }
    return (pattern != NULL);
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

/* ================================================================================ */

class OneShotPatternRunner : public PatternRunner {
public:
  PRCompletion completion;
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

// A simple pattern runner that can run all patterns in the given group by index with manual advancement
class IndexedPatternRunner : public PatternRunner {
protected:
  PatternManager &manager;
  unsigned int patternIndex;
  int groupID;
public:
  IndexedPatternRunner(PatternManager &manager, int startPatternIndex, int groupID=0) 
    : PatternRunner([this](PatternRunner& runner) {
      return this->manager.createPattern(this->patternIndex, this->groupID);
    }), manager(manager), patternIndex(startPatternIndex), groupID(groupID)
     {}

  unsigned int getPatternIndex(int *outGroupID=NULL) {
    if (outGroupID) *outGroupID = groupID;
    return patternIndex;
  }

  // directly set pattern index without changing the running pattern; can cue up a different pattern on next contruction
  void setPatternIndex(unsigned int index) {
    patternIndex = index;
  }

  void setGroup(int group, unsigned int index=0) {
    this->groupID = group;
    runPatternAtIndex(index);
  }

  virtual void runPatternAtIndex(unsigned int index) {
    if (manager.isValidGroupIndex(index, groupID)) {
      stop();
      patternIndex = index;
      start();
    }
  }
  
  void nextPattern() {
    patternIndex = (patternIndex + 1) % manager.patternGroupMap[groupID].size();
    runPatternAtIndex(patternIndex);
  }

  void previousPattern() {
    patternIndex = mod_wrap(patternIndex - 1, manager.patternGroupMap[groupID].size());
    runPatternAtIndex(patternIndex);
  }

  virtual void loop() {
    if (!pattern && !paused) {
      start();
    }
    PatternRunner::loop();
  }
};

// A pattern runner that can crossfade between patterns in the given group and automatically switch between them by timeout
class CrossfadingPatternRunner : public IndexedPatternRunner {
  Pattern *crossfadePattern = NULL;
public:
  unsigned long patternTimeout = 0; // millis, 0 for no autotimeout
  unsigned long crossfadeDuration = 500;
  std::function<void(CrossfadingPatternRunner &)> timeoutRule = [](CrossfadingPatternRunner &) {}; // called on patternTimeout

  CrossfadingPatternRunner(PatternManager &manager, int startPatternIndex, int groupID=0) : IndexedPatternRunner(manager, startPatternIndex, groupID) { }

  virtual void setAlpha(uint8_t alpha) {
    PatternRunner::setAlpha(alpha);
    if (crossfadePattern) {
      crossfadePattern->setAlpha(alpha, true, 4);
    }
  }

  virtual void runPatternAtIndex(unsigned int index) {
    if (crossfadeDuration == 0) {
      IndexedPatternRunner::runPatternAtIndex(index);
    } else if (manager.isValidGroupIndex(index, groupID)) {
      patternIndex = index;
      if (crossfadePattern) {
        stop();
        pattern = crossfadePattern;
      }
      crossfadePattern = constructor(*this);
      if (crossfadePattern) {
        crossfadePattern->alpha = 0;
        crossfadePattern->start();
      }
    }
  }

  virtual void loop() {
    assert(pattern || !crossfadePattern, "inconsistent crossfade state");
    if (!pattern) {
      start();
      pattern->alpha = 0;
    }
    if (pattern && !paused) {
      if (crossfadePattern && crossfadePattern->runTime() > crossfadeDuration) {
        // crossfade done - switch to the pattern we're crossfading in
        logdf("  Pattern crossfade done");
        stop();
        pattern = crossfadePattern;
        crossfadePattern = NULL;
      } else if (!crossfadePattern && patternTimeout != 0 && pattern->runTime() > patternTimeout - crossfadeDuration) {
        // almost pattern timeout - start crossfade
        logdf("Pattern timeout - start crossfade");
        timeoutRule(*this);
        crossfadePattern = constructor(*this);
        if (crossfadePattern) {
          crossfadePattern->alpha = 0;
          crossfadePattern->start();
        }
      }
      if (crossfadePattern) {
        pattern->maxAlpha = dim8_raw(constrain((int)(0xFF - 0xFF * crossfadePattern->runTime() / crossfadeDuration), 0, 0xFF));
      } else {
        pattern->maxAlpha = 0xFF;
      }
    }
    if (crossfadePattern && !paused) {
      crossfadePattern->maxAlpha = dim8_raw(constrain((int)(0xFF * crossfadePattern->runTime() / crossfadeDuration), 0, 0xFF));
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

/* == PatternManager impl ========================================================== */

PatternManager::PatternManager(DrawingContext &ctx) : ctx(ctx) { }

std::shared_ptr<PatternRunner> PatternManager::addRunner(PatternRunner *runner) {
  auto ptr = std::shared_ptr<PatternRunner>(runner);
  runners.push_back(ptr);
  return ptr;
}

void PatternManager::removeRunner(std::shared_ptr<PatternRunner> runner) {
  auto it = std::find(runners.begin(), runners.end(), runner);
  if(it != runners.end()) {
    (*it)->stop();
    runners.erase(it);
  } else {
    assert(false, "Attempt to remove a runner that was not found");
  }
}

void PatternManager::removeAllRunners() {
  runners.clear();
}

// Test pattern runs by default and in exclusive mode
template<class T>
PatternRunner& PatternManager::setTestRunner() {
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

bool PatternManager::hasTestRunner() {
  return (testRunner != NULL);
}

// Add a pattern class to the patterns list for the random and indexed runners to use. Returns group pattern index.
template<class T>
unsigned int PatternManager::registerPattern(int groupID) {
  int patternIndex = patternConstructors.size();
  patternConstructors.push_back(&(construct<T>));
  return groupAddPatternIndex(patternIndex, groupID);
}

// Pattern Groups
unsigned int PatternManager::groupAddPatternIndex(unsigned int patternIndex, int groupID) {
  patternGroupMap[groupID].push_back(patternIndex);
  return patternGroupMap[groupID].size()-1;
}

void PatternManager::groupRemovePatternIndex(unsigned int patternIndex, int groupID) {
  auto group = patternGroupMap[groupID];
  auto it = find(group.begin(), group.end(), patternIndex);
  assert(it != group.end(), "can't find patternIndex to remove it from group");
  if (it != group.end()) {
    group.erase(it);
  }
}

std::vector<int> PatternManager::patternIndexesInGroup(int groupID) {
  return patternGroupMap[groupID];
}

Pattern *PatternManager::createPattern(unsigned int patternIndex, int groupID) {
  auto group = patternGroupMap[groupID];
  assert(patternIndex < group.size(), "createPattern: Pattern %i group %i out of bounds size %i for group", patternIndex, groupID, group.size());
  if (patternIndex < group.size()) {
    return patternConstructors[group[patternIndex]]();
  }
  return (Pattern*)nullptr;
}

bool PatternManager::isValidGroupIndex(unsigned int patternIndex, int groupID) {
  auto group = patternGroupMap[groupID];
  return patternIndex < group.size();
}

// Convenience

// Creates a random pattern selected from the given groupID and immediately runs it; destroys the pattern once it's stopped. Dims other patterns by dimAmount if highest priority.
// Only intended to be used with patterns that end on their own.
std::shared_ptr<PatternRunner> PatternManager::runRandomOneShotFromGroup(int groupID, uint8_t priority, uint8_t dimAmount) {
  auto patternGroup = this->patternGroupMap[groupID];
  auto ctorIndex = patternGroup[random16(patternGroup.size())];
  auto ctor = this->patternConstructors[ctorIndex];
  return runOneShotPattern([ctor](PatternRunner &runner) { return ctor(); }, 
                    priority, dimAmount);
}

// Creates pattern with constructor immediately runs it; destroys the pattern once it's stopped. Dims other patterns by dimAmount if highest priority.
// Only intended to be used with patterns that end on their own.
std::shared_ptr<PatternRunner> PatternManager::runOneShotPattern(PRConstructor constructor, uint8_t priority, uint8_t dimAmount, PRCompletion completion) {
  OneShotPatternRunner *runner = new OneShotPatternRunner(constructor);
  runner->completion = completion;
  runner->dimAmount = dimAmount;
  runner->priority = priority;
  runner->completion = completion;
  auto ptr = addRunner(runner);
  runner->start();
  return ptr;
}

// Creates pattern with constructor immediately runs it; destroys the pattern once it's stopped. Dims other patterns by dimAmount if highest priority.
// Only intended to be used with patterns that end on their own.
template<class T>
std::shared_ptr<PatternRunner> PatternManager::runOneShotPattern(uint8_t priority, uint8_t dimAmount, PRCompletion completion) {
  return PatternManager::runOneShotPattern([](PatternRunner&) { return construct<T>(); }, priority, dimAmount, completion);
}

// When runCondition > 0, creates pattern with constructor and fades it in using runCondition return value. Dims other patterns by dimAmount if highest priority.
ConditionalPatternRunner* PatternManager::setupConditionalRunner(PRConstructor constructor, PRPredicate runCondition, uint8_t priority, uint8_t dimAmount) {
  ConditionalPatternRunner *runner = new ConditionalPatternRunner(constructor, runCondition);
  runner->priority = priority;
  runner->dimAmount = dimAmount;
  addRunner(runner);
  return runner;
}

// When runCondition > 0, creates given pattern class and fades it in using runCondition return value. Dims other patterns by dimAmount if highest priority.
template<class T>
inline ConditionalPatternRunner* PatternManager::setupConditionalRunner(PRPredicate runCondition, uint8_t priority, uint8_t dimAmount) {
  return setupConditionalRunner([](PatternRunner&) { return construct<T>(); }, runCondition, priority, dimAmount);
}

// Start a random pattern from a pattern group with optional crossfade
CrossfadingPatternRunner* PatternManager::setupRandomRunner(unsigned long runDuration, unsigned long crossfadeDuration, int groupID) {
  auto patternGroup = this->patternGroupMap[groupID];
  unsigned int startPatternIndex = random16(patternGroup.size());

  CrossfadingPatternRunner *runner = new CrossfadingPatternRunner(*this, startPatternIndex, groupID);
  runner->patternTimeout = 40*1000;
  runner->timeoutRule = [this](CrossfadingPatternRunner &xr) {
    int groupID;
    auto curIndex = xr.getPatternIndex(&groupID);
    auto patternGroup = this->patternGroupMap[groupID];
    if (patternGroup.size() < 2) {
      return;
    }
    unsigned int nextPattern;
    do {
      nextPattern = random16(patternGroup.size());
    } while (nextPattern == curIndex);
    xr.setPatternIndex(nextPattern);
  };
  
  runner->patternTimeout = runDuration;
  runner->crossfadeDuration = crossfadeDuration;
  addRunner(runner);
  return runner;
}

// Start a pattern by list index
IndexedPatternRunner *PatternManager::setupIndexedRunner(unsigned int startIndex, int groupID) {
  IndexedPatternRunner *runner = new IndexedPatternRunner(*this, startIndex, groupID);
  addRunner(runner);
  return runner;
}

// Start a pattern by list index with crossfading
CrossfadingPatternRunner *PatternManager::setupCrossfadingRunner(unsigned int startIndex, int groupID) {
  CrossfadingPatternRunner *runner = new CrossfadingPatternRunner(*this, startIndex, groupID);
  addRunner(runner);
  return runner;
}

// Returns a priority higher than what's currently running
uint8_t PatternManager::highestPriority() {
  uint8_t maxPriority = 0;
  for (auto runner : runners) {
    if (runner->priority > maxPriority) {
      maxPriority = runner->priority;
    }
  }
  assert(maxPriority < 0xFF, "already at max priority");
  return min(0xFF, maxPriority + 1);
}

void PatternManager::setup() { }

void PatternManager::loop() {
  ctx.leds.fill_solid(CRGB::Black);
  
  uint8_t maxPriority = 0;
  uint8_t priorityDimAmount = 0;
  if (!testRunner) {
    for (auto runner : runners) {
      runner->loop();
      if (runner->priority > maxPriority && runner->pattern && !runner->paused) {
        // simplified: only considers dimming from the max priority runner.
        maxPriority = runner->priority;
        priorityDimAmount = runner->dimAmount;
      }
    }
    for (auto runner : runners) {
      runner->setAlpha(0xFF - (runner->priority < maxPriority ? priorityDimAmount : 0));
      runner->draw(ctx);
    }
  } else {
    // special-case testRunner so that no other patterns are ever run
    testRunner->loop();
    testRunner->setAlpha(0xFF);
    testRunner->draw(ctx);
  }
  for (auto it = runners.begin(); it < runners.end(); ) {
    if ((*it)->complete) {
      logdf("Removing a complete runner");
      runners.erase(it);
    } else {
      ++it;
    }
  }
}


#endif
