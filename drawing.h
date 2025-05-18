
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
    src.nscale8(brightness);
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
  unsigned long lastTick = 0;
  uint16_t fadeDownAccum = 0;
public:
  PixelSetType<COUNT> leds;
  const uint16_t count;
  PixelStorage() : count(COUNT) {
    leds.fill_solid(CRGB::Black);
  }
  
  void blendIntoContext(PixelStorage<COUNT, PixelType, PixelSetType> &otherContext, BlendMode blendMode, uint8_t brightness=0xFF) {
    if (brightness > 0) {
      // TODO: if blendMode is sourceOver can we use memcpy?
      assert(otherContext.leds.size() == this->leds.size(), "context blending requires same-size buffers");
      for (int i = 0; i < leds.size(); ++i) {
        otherContext.set_px(leds[i], i, blendMode, brightness);
      }
    }
  }

  void point(unsigned int index, PixelType c, BlendMode blendMode = blendSourceOver, uint8_t brightness=0xFF) {
    assert(index < count, "index=%u is out of range [0,%u]", index, count-1);
    if (index < count) {
      set_px(c, index, blendMode, brightness);
    }
  }

  // framerate-invariant high-granularity fadedown
  void fadeToBlackBy16(uint16_t fadeDown) {
    unsigned long mils = millis();
    if (lastTick) {
      fadeDownAccum += fadeDown * (mils - lastTick);
      uint8_t fadeDownThisFrame = fadeDownAccum >> 8;
      this->leds.fadeToBlackBy(fadeDownThisFrame);
      fadeDownAccum -= fadeDownThisFrame << 8;
    }
    lastTick = mils;
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
