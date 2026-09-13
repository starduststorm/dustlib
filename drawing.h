
#ifndef DRAWING_H
#define DRAWING_H

#include <stack>
#include <FastLED.h>
#include <util.h>

enum BlendMode {
  blendSourceOver, blendBrighten, blendDarken, blendSubtract, blendMultiply, blendScreen, 
};

struct DrawStyle {
public:
  BlendMode blendMode = blendSourceOver;
};

template<int COUNT, class PixelType=CRGB, template<int SIZE> typename PixelSetType=CRGBArray>
class PixelStorage {
private:
  inline void set_px(PixelType src, int index, BlendMode blendMode, uint8_t brightness) {
    if (brightness != 0xFF) {
      src.nscale8(brightness);
    }
    PixelType dst = leds[index];
    switch (blendMode) {
      case blendSourceOver:
        leds[index] = src; break;
      case blendBrighten:
        leds[index] = PixelType(std::max(src.r, dst.r), std::max(src.g, dst.g), std::max(src.b, dst.b)); break;
      case blendDarken:
        leds[index] = PixelType(std::min(src.r, dst.r), std::min(src.g, dst.g), std::min(src.b, dst.b)); break;
      case blendSubtract:
        leds[index] = dst - src; break;
      case blendMultiply:
        leds[index] = src.scale8(dst); break;
      case blendScreen:
        // 1 - [(1-dst) x (1-src)]
        leds[index] = CRGB::White - (CRGB::White - dst).scale8(CRGB::White - dst); break;

    }
  }

  template<BlendMode MODE>
  void blendLoop(PixelStorage<COUNT, PixelType, PixelSetType> &otherContext, uint8_t brightness) {
    for (int i = 0; i < COUNT; ++i) {
      otherContext.set_px(leds[i], i, MODE, brightness);
    }
  }

  unsigned long lastFadeTick = 0;
  // Sub-count fade precision: one fractional byte per channel, allocated on first fadeToBlackBy16().
  // The visible value of a channel is leds[i].raw[c] + fadeFrac[3*i+c]/256.
  uint8_t *fadeFrac = nullptr;
public:
  PixelSetType<COUNT> leds;
  const uint16_t count;
  // optimization: instead of clearing the buffer every frame, mark it as pendingClear, then the next 
  // blend can overwrite it (e.g. sourceOver) or by clearing it first if necessary.
  // allows a single pattern to memcpy in its drawing.
  bool pendingClear = false;

  PixelStorage() : count(COUNT) {
    leds.fill_solid(CRGB::Black);
  }
  ~PixelStorage() {
    delete[] fadeFrac;
  }
  PixelStorage(const PixelStorage &) = delete;
  PixelStorage &operator=(const PixelStorage &) = delete;

  void resolvePendingClear() {
    if (pendingClear) {
      leds.fill_solid(CRGB::Black);
      pendingClear = false;
    }
  }

  void blendIntoContext(PixelStorage<COUNT, PixelType, PixelSetType> &otherContext, BlendMode blendMode, uint8_t brightness=0xFF) {
    if (brightness == 0) {
      return;
    }
    // sourceOver overwrites unconditionally; brighten against a known-black destination is also a plain copy
    if (blendMode == blendSourceOver ||
        (otherContext.pendingClear && blendMode == blendBrighten)) {
      if (brightness == 0xFF) {
        memcpy(&otherContext.leds[0], &this->leds[0], COUNT * sizeof(PixelType));
      } else {
        for (int i = 0; i < COUNT; ++i) {
          PixelType src = leds[i];
          src.nscale8(brightness);
          otherContext.leds[i] = src;
        }
      }
      otherContext.pendingClear = false;
      return;
    }
    otherContext.resolvePendingClear();
    switch (blendMode) {
      case blendSourceOver: blendLoop<blendSourceOver>(otherContext, brightness); break;
      case blendBrighten:   blendLoop<blendBrighten>(otherContext, brightness); break;
      case blendDarken:     blendLoop<blendDarken>(otherContext, brightness); break;
      case blendSubtract:   blendLoop<blendSubtract>(otherContext, brightness); break;
      case blendMultiply:   blendLoop<blendMultiply>(otherContext, brightness); break;
      case blendScreen:     blendLoop<blendScreen>(otherContext, brightness); break;
    }
  }

  void point(unsigned int index, PixelType c, BlendMode blendMode = blendSourceOver, uint8_t brightness=0xFF) {
    assert(index < count, "index=%u is out of range [0,%u]", index, count-1);
    resolvePendingClear();
    if (index < count) {
      set_px(c, index, blendMode, brightness);
    }
  }

  // Framerate-invariant, hue-preserving fadedown.
  // fadeDown is in 1/256 counts per millisecond
  // Each channel is faded as 8 integer bits + 8 fractional bits kept in fadeFrac
  void fadeToBlackBy16(uint16_t fadeDown) {
    resolvePendingClear();
    unsigned long now = micros();
    if (lastFadeTick) {
      uint32_t elapsedMicros = (uint32_t)(now - lastFadeTick);
      // fraction to remove this frame, in 1/65536 units: (fadeDown/256 counts per ms) / 256 counts
      uint32_t removed = ((uint64_t)fadeDown * elapsedMicros) / 1000;
      if (removed >= 65536) {
        leds.fill_solid(CRGB::Black);
        if (fadeFrac) memset(fadeFrac, 0, COUNT * 3);
      } else if (removed > 0) {
        if (!fadeFrac) {
          fadeFrac = new uint8_t[COUNT * 3]();
        }
        const uint32_t keep = 65536 - removed;
        uint8_t *frac = fadeFrac;
        for (int i = 0; i < COUNT; ++i, frac += 3) {
          uint8_t *px = leds[i].raw;
          for (int c = 0; c < 3; ++c) {
            uint32_t x = ((uint32_t)px[c] << 8) | frac[c];
            if (x) {
              x = (x * keep) >> 16;
              px[c] = x >> 8;
              frac[c] = x & 0xFF;
            }
          }
        }
      }
    }
    lastFadeTick = now;
  }
};

/* Floating-point pixel buffer support */

typedef struct FCRGB {
  union {
    struct {
      union {
        float r;
        float red;
      };
      union {
        float g;
        float green;
      };
      union {
        float b;
        float blue;
      };
    };
    float raw[3];
  };
public:
  FCRGB() { }
  FCRGB(CRGB color) : red(color.r), green(color.g), blue(color.b) { }
  FCRGB(float r, float g, float b) : red(r), green(g), blue(b) { }
  inline float& operator[] (uint8_t x) __attribute__((always_inline)) {
    return raw[x];
  }
} FCRGB;

template<int SIZE>
class FCRGBArray {
  FCRGB entries[SIZE];
public:
  inline FCRGB& operator[] (uint16_t x) __attribute__((always_inline)) {
    return entries[x];
  };
};

#endif
