#ifndef PARTICLES_H
#define PARTICLES_H

#include <FastLED.h>
#include <vector>
#include <set>
#include <functional>
#include <optional>

#include <drawing.h>
#include <mapping.h>
#include <paletting.h>
#include <util.h>

// a lil patternlet to run a particle simulation on an adjacency graph
// supports max 255 particles for now

template <int SIZE>
class ParticleSim;

  // note: suboptimal packing when PixelIndex is 2 bytes
  // TODO: Particle needs to be specialized / reduced features to be backported to 32KB ram chips e.g. samd21g
struct Particle {
  template <int SIZE>
  friend class ParticleSim;
private:
  unsigned long birthmilli;
  unsigned long lastMove = 0; // when px was updated
  PixelIndex lastPx;
public:
  PixelIndex px; // current particle position (or start of fadeup-chain)

  uint8_t brightness = 0xFF;
  uint8_t speed = 0; // pixels/second

  CRGB color;
  uint8_t colorIndex; // storage only
  
  unsigned long lifespan;
  EdgeTypesQuad directions; // quad allows four priority levels

protected:
  std::optional<PixelIndex> continueToPx; // stash for when particle is planning to move to a continueTo=true Edge
  bool alive = true; // when a particle dies, its fadeup trail still needs to be completed so we cannot remove it immediately
  uint8_t fadeUpDistance = 0;
  std::optional<PixelIndex> *fadeHistory = NULL; // when fading up, particle.px tracks the start of the fade chain, fadeHistory tracks pixels that have not yet reached full brightness
public:

  Particle(int px, EdgeTypesQuad directions, unsigned long lifespan) 
    : px(px), directions(directions), lifespan(lifespan) {
    reset();
  }

  ~Particle() noexcept {
    delete [] fadeHistory;
  }

  Particle(const Particle &other) noexcept :
    birthmilli(other.birthmilli),
    lifespan(other.lifespan),
    alive(other.alive),
    color(other.color),
    colorIndex(other.colorIndex),
    px(other.px),
    lastPx(other.lastPx),
    continueToPx(other.continueToPx),
    directions(other.directions),
    brightness(other.brightness),
    speed(other.speed),
    lastMove(other.lastMove),
    fadeUpDistance(other.fadeUpDistance) {
      if (this != &other) {
        fadeHistory = new std::optional<PixelIndex>[fadeUpDistance];
        for (int i = 0; i < fadeUpDistance; ++i) {
          fadeHistory[i] = other.fadeHistory[i];
        }
      }
  }

  Particle(Particle &&other) noexcept {
    *this = std::move(other);
  }

  Particle &operator=(Particle &&other) noexcept {
    if (this != &other) {
      birthmilli = other.birthmilli;
      lifespan = other.lifespan;
      alive = other.alive;
      
      color = other.color;
      colorIndex = other.colorIndex;
      brightness = other.brightness;
      speed = other.speed;
      lastMove = other.lastMove,

      px = other.px;
      lastPx = other.lastPx;
      continueToPx = other.continueToPx;
      directions = other.directions;

      fadeUpDistance = other.fadeUpDistance;
      delete [] fadeHistory;
      fadeHistory = other.fadeHistory;
      other.fadeHistory = NULL;
    }
    return *this;
  }

  Particle &operator=(const Particle &other) {
    if (this == &other) return *this;

    birthmilli = other.birthmilli;
    lifespan = other.lifespan;
    alive = other.alive;
    
    color = other.color;
    colorIndex = other.colorIndex;
    brightness = other.brightness;
    speed = other.speed;
    lastMove = other.lastMove,

    px = other.px;
    lastPx = other.lastPx;
    continueToPx = other.continueToPx;
    directions = other.directions;

    fadeUpDistance = other.fadeUpDistance;
    delete [] fadeHistory;
    fadeHistory = new std::optional<PixelIndex>[fadeUpDistance];
    for (int d = 0; d < fadeUpDistance; ++d) {
      fadeHistory[d] = other.fadeHistory[d];
    }
    return *this;
  }

  void reset() {
    birthmilli = millis();
    color = CHSV(random8(), 0xFF, 0xFF);
  }

  void clearFadeHistory() {
    for (unsigned i = 0; i < fadeUpDistance; ++i) {
      fadeHistory[i] = std::nullopt;
    }
  }

  void setFadeUpDistance(unsigned distance) {
    fadeUpDistance = distance;
    delete [] fadeHistory;
    if (distance > 0) {
      fadeHistory = new std::optional<PixelIndex>[distance];
      clearFadeHistory();
    } else {
      fadeHistory = NULL;
    }
  }

  // Bit age, capped at lifespan
  unsigned long age() {
    return min(millis() - birthmilli, lifespan ?: millis() - birthmilli);
  }
  // Bit age as a byte, or 0 if no max lifespan
  uint8_t ageByte() {
    if (lifespan > 0) {
      return 0xFF * age() / lifespan;
    }
    return 0;
  }
protected:
  unsigned long exactAge() {
    return millis() - birthmilli;
  }
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

template <int SIZE> // SIZE is number of pixels
class ParticleSim {
public:
  typedef enum : uint8_t { random, priority, split } FlowRule;
  typedef enum : uint8_t { maintainPopulation, manualSpawn } SpawnRule;

  bool preventReverseFlow = false; // if true, prevent particles from naturally flowing A->B->A
  bool followContinueTo = false;   // enable following Edges with continueTo=true when in priority mode
  bool requireExactEdgeTypeMatch = false; // require `Particle.directions == (Edge.types & Particle.directions)` rather than `Particle.direcitons & Edge.types`

  std::vector<Particle> particles;
  uint8_t maxSpawnPopulation; // number of particles to spawn when spawnRule is maintainPopulation
  uint8_t maxSpawnPerSecond; // limit how fast new particles are spawned, 0 = no limit (defaults to 1000 * maxSpawnPopulation / lifespan)
  uint8_t startingSpeed; // for new particles ; pixels/second
  std::vector<EdgeTypesQuad> flowDirections; // for new particles. quad allows four priority levels

  unsigned long lifespan = 0; // in milliseconds, forever if 0

  FlowRule flowRule = random;
  SpawnRule spawnRule = maintainPopulation;

  EdgeTypes splitDirections = Edge::all; // if flowRule is split, which directions are allowed to split
  
  const std::vector<PixelIndex> *spawnPixels = NULL; // list of pixels to automatically spawn particles on
  const std::set<PixelIndex> *allowedPixels = NULL; // set of pixels that particles are allowed to travel to

  std::function<void(Particle &)> handleNewParticle = [](Particle &particle){};                               // called upon particle creation
  std::function<void(Particle &, PixelIndex)> handleUpdateParticle = [](Particle &particle, uint8_t index){}; // called once per frame per live particle
  std::function<void(Particle &)> handleKillParticle = [](Particle &particle){};                              // called upon particle death

  ParticleSim(Graph &graph, PixelStorage<SIZE> &ctx, uint8_t maxSpawnPopulation, uint8_t startingSpeed, unsigned long lifespan, std::vector<EdgeTypesQuad> flowDirections)
    : graph(graph), ctx(ctx), maxSpawnPopulation(maxSpawnPopulation), startingSpeed(startingSpeed), flowDirections(flowDirections), lifespan(lifespan) {
    particles.reserve(maxSpawnPopulation);
    maxSpawnPerSecond = 1000 * maxSpawnPopulation / lifespan;
  };

  uint16_t fadeDown = 4 << 8; // fadeToBlackBy units per 1/256 millisecond

private:
  PixelStorage<SIZE> &ctx;
  Graph &graph;

  unsigned long lastTick = 0;
  unsigned long lastParticleSpawn = 0;
  uint8_t fadeUpDistance = 0; // fade up n pixels ahead of particle motion

  PixelIndex spawnLocation() {
    if (spawnPixels) {
      return spawnPixels->at(random8()%spawnPixels->size());
    }
    return random16()%ctx.leds.size();
  }

  Particle &makeParticle(Particle *fromParticle=NULL) {
    assert(particles.size() < 255, "Too many particles");
    // the particle directions at the Particles level may contain multiple options, choose one at random for this particle
    EdgeTypesQuad directionsForParticle = flowDirections[random8(flowDirections.size())];

    if (fromParticle) {
      particles.emplace_back(*fromParticle);
      particles.back().clearFadeHistory();
    } else {
      particles.emplace_back(spawnLocation(), directionsForParticle, lifespan);
      particles.back().setFadeUpDistance(fadeUpDistance);
      particles.back().speed = startingSpeed;
    }
    return particles.back();
  }

  void eraseParticle(uint8_t index) {
    particles.erase(particles.begin() + index);
  }

  void killParticle(uint8_t index) {
    particles[index].alive = false;
    handleKillParticle(particles[index]);
    if (particles[index].fadeUpDistance == 0) {
      eraseParticle(index);
    }
  }

  void splitParticle(Particle &particle, PixelIndex toPx) {
    // logf("Splitting particle at %i to %i", particle.px, toPx);
    assert(flowRule == split, "are we splitting or not");
    Particle &split = makeParticle(&particle);
    split.px = toPx;
    split.lastPx = particle.px;
  }

  bool isIndexAllowedForParticle(Particle &particle, PixelIndex index) {
    if (preventReverseFlow && index == particle.lastPx) {
      return false;
    }
    if (allowedPixels) {
      return allowedPixels->end() != allowedPixels->find(index);
    }
    return true;
  }

  std::vector<Edge> edgeCandidates(Particle &particle) {
    std::vector<Edge> nextEdges;
    switch (flowRule) {
      case priority: {
        auto adj = ledgraph.adjacencies(particle.px, particle.directions, requireExactEdgeTypeMatch);
        // logf("edgeCandidates: particle@px %i has %i adjacencies matching directions 0x%x", particle.px, adj.size(), particle.directions.quad);
        for (auto edge : adj) {
          // logf("  checking adj %i->%i for edge types 0x%x...", (int)edge.from, (int)edge.to, (int)edge.types);
          if (isIndexAllowedForParticle(particle, edge.to)) {
            if (followContinueTo && edge.continueTo) {
              // continueToPx: stash off the pixel to continue across an intersection where following a single edgeType may be ambiguous
              particle.continueToPx = edge.to;
            } else if (followContinueTo && particle.continueToPx == edge.to) {
              // follow the stashed edge
              nextEdges.clear();
              nextEdges.push_back(edge);
              particle.continueToPx.reset();
              break;
            } else if (!edge.continueTo) { // regular edge
              nextEdges.push_back(edge);
            }
          }
        }
        if (nextEdges.size() > 0) {
          // only ever follow one edge in priority mode
          nextEdges = {nextEdges[0]};
        }
        break;
      }
      case random:
      case split: {
        auto allAdj = ledgraph.adjacencies(particle.px, particle.directions, requireExactEdgeTypeMatch);
        std::vector<Edge> allowedEdges;
        for (auto edge : allAdj) {
          if (isIndexAllowedForParticle(particle, edge.to) && edge.types && !edge.continueTo) {
            allowedEdges.push_back(edge);
          }
        }
        if (flowRule == split) {
          if (allowedEdges.size() == 1) {
            // flow normally if we're not actually splitting
            nextEdges.push_back(allowedEdges.front());
          } else {
            // split along all allowed split directions, or none if none are allowed
            for (Edge nextEdge : allowedEdges) {
              if ((splitDirections & nextEdge.types) && !nextEdge.continueTo) {
                nextEdges.push_back(nextEdge);
              }
            }
          }
        } else if (allowedEdges.size() > 0) {
          nextEdges.push_back(allowedEdges.at(random8()%allowedEdges.size()));
        }
        break;
      }
    }
    return nextEdges;
  }

  bool flowParticle(uint8_t index) {
    if (fadeUpDistance > 0) {
      // scoot the fade-up history
      for (int d = fadeUpDistance-1; d >= 1; --d) {
        particles[index].fadeHistory[d] = particles[index].fadeHistory[d-1];
      }
      particles[index].fadeHistory[0] = (particles[index].alive ? std::optional<PixelIndex>(particles[index].px) : std::nullopt);
    }
    
    if (!particles[index].alive) {
      return false;
    }

    std::vector<Edge> nextEdges = edgeCandidates(particles[index]);
    if (nextEdges.size() == 0) {
      // leaf behavior
      // logf("  no path for particle %i", index);
      killParticle(index);
      return false;
    } else {
      std::set<PixelIndex> toVertexes; // dedupe
      for (unsigned i = 0; i < nextEdges.size(); ++i) {
        toVertexes.insert(nextEdges[i].to);
      }
      if (toVertexes.size() > 1) {
        for (PixelIndex idx : toVertexes) {
          splitParticle(particles[index], idx);
        }
      }
      particles[index].lastPx = particles[index].px;
      particles[index].px = nextEdges.front().to;
    }
    return true;
  }

public:
  void dumpParticles() {
    logf("--------");
    logf("There are %i particles", particles.size());
    for (unsigned b = 0; b < particles.size(); ++b) {
      Particle &particle = particles[b];
      logf("Particle %i: px=%i, birthmilli=%lu, colorIndex=%u, speed=%u, directions=%x", b, particle.px, particle.birthmilli, particle.colorIndex, particle.speed, particle.directions.quad);
    }
    logf("--------");
  }

  void update() {
    unsigned long mils = millis();

    bool firstFrameForParticle[particles.size()];
    for (int i = 0 ; i < particles.size(); ++i) {
      firstFrameForParticle[i] = (particles[i].lastMove == 0);
    }

    ctx.fadeToBlackBy16(fadeDown);
    
    if (spawnRule == maintainPopulation) {
      for (unsigned b = particles.size(); b < maxSpawnPopulation; ++b) {
        if (maxSpawnPerSecond != 0 && mils - lastParticleSpawn < 1000 / maxSpawnPerSecond) {
          continue;
        }
        addParticle();
        lastParticleSpawn = mils;
      }
    }

    // // Update! // //
    
    for (int i = particles.size() - 1; i >= 0; --i) {
      Particle &p = particles[i];
      if (firstFrameForParticle[i]) {
        // don't flow particles on the first frame. this allows pattern code to make their own particles that are displayed before being flowed
        p.lastMove = mils;
      } else if (mils - p.lastMove > 1000/p.speed) {
        if (flowParticle(i)) { // possibly destroys the particle
          if (p.lifespan != 0 && p.exactAge() > p.lifespan) {
            killParticle(i);
          } else {
            if (mils - p.lastMove > 2000/p.speed) {
              p.lastMove = mils;
            } else {
              // This helps avoid time drift, which for some reason can make one device run consistently faster than another
              p.lastMove += 1000/p.speed;
            }
          }
        }
      }
    }

    // // Draw! // //

    if (fadeUpDistance == 0) {
      for (Particle &p : particles) {
        if (!p.alive) continue;
        uint8_t blendAmount = 0xFF / (fadeUpDistance+1);
        CRGB newColor = CRGB(p.color).nscale8(p.brightness);
        ctx.point(p.px, newColor, blendBrighten, blendAmount);
      }
    } else { // fade up
      for (int index = particles.size() - 1; index >= 0; --index) {
        Particle &p = particles[index];
        bool activelyFading = false;
        // logf("  p %i at px %i", index, p.px);
        // loglf("fade up for p %i: ", index);
        for (int d = 0; d < p.fadeUpDistance; ++d) {
          if (p.fadeHistory[d].has_value()) {
            PixelIndex px = p.fadeHistory[d].value();
            activelyFading = true;

            uint16_t interMoveScale = (firstFrameForParticle[index] ? 0 : (1<<16-1) * (mils - p.lastMove) * p.speed / 1000);
            uint8_t blendAmount = d * 0xFF / p.fadeUpDistance + scale16(0xFF/fadeUpDistance, interMoveScale);
            blendAmount = scale8(blendAmount, p.brightness);
            ctx.point(px, p.color, blendBrighten, blendAmount);
          }
        }
        if (!p.alive && !activelyFading) {
          eraseParticle(index);
        }
      }
    }

    uint8_t i = 0;
    for (Particle &p : particles) {
      if (p.alive) {
        handleUpdateParticle(p, i++);
      }
    }

    lastTick = mils;
  };

  Particle &addParticle() {
    Particle &newbit = makeParticle();
    handleNewParticle(newbit);
    return newbit;
  }

  void removeParticle(uint8_t index) {
    killParticle(index);
  }

  void removeAllParticles() {
    particles.clear();
  }

  void resetParticleColors(ColorManager *colorManager) {
    for (Particle &p : particles) {
      p.color = colorManager->getPaletteColor(p.colorIndex, p.color.getAverageLight());
    }
  }

  void setAllSpeed(uint8_t newSpeed) {
    this->startingSpeed = newSpeed;
    for (Particle &p : particles) {
      p.speed = newSpeed;
    }
  }
  
  void setFadeUpDistance(uint8_t distance) {
    if (distance != fadeUpDistance) {
      for (Particle &p : particles) {
        p.setFadeUpDistance(distance);
      }
      fadeUpDistance = distance;
    }
  }
};

#endif
