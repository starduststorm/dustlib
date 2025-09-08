#ifndef PALETTING_H
#define PALETTING_H

#include <FastLED.h>
#include "ext-palettes.h"
// Colors are pulled from publically available flag values, then refined to render better on my SMD LEDs

DEFINE_GRADIENT_PALETTE( Trans_Flag_gp ) {
  0,   0x2A, 0x9F, 0xFA,
  50,  0x2A, 0x9F, 0xFA,
  51,  0xF1, 0x55, 0x70,
  101, 0xF1, 0x55, 0x70,
  102, 0xFF, 0xFF, 0xFF,
  152, 0xFF, 0xFF, 0xFF,
  153, 0xF1, 0x55, 0x70,
  203, 0xF1, 0x55, 0x70,
  204, 0x2A, 0x9F, 0xFA,
  255, 0x2A, 0x9F, 0xFA,
};

DEFINE_GRADIENT_PALETTE( Bi_Flag_gp ) {
  0,   0xB6, 0x02, 0x40,
  101, 0xB6, 0x02, 0x40,
  102, 0x6E, 0x07, 0xD7,
  152, 0x6E, 0x07, 0xD7,
  153, 0x00, 0x38, 0xD8,
  255, 0x00, 0x38, 0xD8,
};

DEFINE_GRADIENT_PALETTE( Lesbian_Flag_gp ) {
  0,   0xD6, 0x29, 0x00,
  50,  0xD6, 0x29, 0x00,
  51,  0xCF, 0x5F, 0x20,
  101, 0xCF, 0x5F, 0x20,
  102, 0xFF, 0xFF, 0xFF,
  152, 0xFF, 0xFF, 0xFF,
  153, 0xD1, 0x50, 0x60,
  204, 0xD1, 0x55, 0x70,
  205, 0x90, 0x00, 0x52,
  255, 0x90, 0x00, 0x52,
};

DEFINE_GRADIENT_PALETTE( Pride_Flag_gp ) {
  0,   0xF4, 0x03, 0x03,
  42,  0xF4, 0x03, 0x03,
  43,  0xCF, 0x35, 0x00,
  85,  0xCF, 0x35, 0x00,
  86,  0xFF, 0xED, 0x00,
  127, 0xFF, 0xED, 0x00,
  128, 0x00, 0xC0, 0x26,
  170, 0x00, 0xC0, 0x26,
  171, 0x00, 0x2D, 0xFF,
  212, 0x00, 0x2D, 0xFF,
  213, 0x75, 0x07, 0xB7,
  255, 0x75, 0x07, 0xB7,
};

DEFINE_GRADIENT_PALETTE( Ace_Flag_gp ) {
  0,   0x40, 0x40, 0x40,
  63,  0x40, 0x40, 0x40,
  64,  0x71, 0x00, 0x81,
  127, 0x71, 0x00, 0x81,
  128, 0xFF, 0xFF, 0xFF,
  195, 0xFF, 0xFF, 0xFF,
  196, 0x71, 0x00, 0x81,
  255, 0x71, 0x00, 0x81,
};

DEFINE_GRADIENT_PALETTE( Enby_Flag_gp ) {
  0,   0xFF, 0xF4, 0x30,
  85,  0xFF, 0xF4, 0x30,
  86,  0xFF, 0xFF, 0xFF,
  170, 0xFF, 0xFF, 0xFF,
  171, 0x6E, 0x07, 0xD7,
  255, 0x6E, 0x07, 0xD7,
};

DEFINE_GRADIENT_PALETTE( Genderqueer_Flag_gp ) {
  0,   0x8E, 0x20, 0xD7,
  85,  0x8E, 0x20, 0xD7,
  86,  0xFF, 0xFF, 0xFF,
  170, 0xFF, 0xFF, 0xFF,
  171, 0x28, 0x82, 0x10,
  255, 0x28, 0x82, 0x10,
};

DEFINE_GRADIENT_PALETTE( Intersex_Flag_gp ) {
  0,   0x6E, 0x07, 0xD7,
  63,  0x6E, 0x07, 0xD7,
  64,  0xFF, 0xFF, 0x00,
  127, 0xFF, 0xFF, 0x00,
  128, 0x6E, 0x07, 0xD7,
  191, 0x6E, 0x07, 0xD7,
  192, 0xFF, 0xFF, 0x00,
  255, 0xFF, 0xFF, 0x00,
};

// 2-band variant
// DEFINE_GRADIENT_PALETTE( Intersex_Flag_gp ) {
//   0,   0x6E, 0x07, 0xD7,
//   127,  0x6E, 0x07, 0xD7,
//   128, 0xFF, 0xFF, 0x00,
//   255, 0xFF, 0xFF, 0x00,
// };

DEFINE_GRADIENT_PALETTE( Pan_Flag_gp ) {
  0,   0xFF, 0x1B, 0x8D,
  85,  0xFF, 0x1B, 0x8D,
  86,  0xFF, 0xDA, 0x00,
  170, 0xFF, 0xDA, 0x00,
  171, 0x1B, 0xB3, 0xFF,
  255, 0x1B, 0xB3, 0xFF,
};

const TProgmemRGBGradientPaletteRef gPrideFlagPalettes[] = {
  Trans_Flag_gp,
  Enby_Flag_gp,
  Genderqueer_Flag_gp,
  Intersex_Flag_gp,
  Pride_Flag_gp,
  Bi_Flag_gp,
  Lesbian_Flag_gp,
  Ace_Flag_gp,
  Pan_Flag_gp,
};

const uint8_t gPridePaletteCount =
  sizeof( gPrideFlagPalettes) / sizeof( TProgmemRGBGradientPaletteRef );

uint8_t paletteBandCount(TProgmemRGBGradientPaletteRef progpal) {
  // cribbed from FastLED for counting entries in DEFINE_GRADIENT_PALETTE 
  TRGBGradientPaletteEntryUnion* progent = (TRGBGradientPaletteEntryUnion*)(progpal);
  TRGBGradientPaletteEntryUnion u;
  uint16_t count = 0;
  do {
      u.dword = FL_PGM_READ_DWORD_NEAR(progent + count);
      ++count;
  } while ( u.index != 255);
        
  return count >> 1; // flag colors = 1/2 entries
}

//

const TProgmemRGBGradientPaletteRef gGradientPalettes[] = {
  Sunset_Real_gp,
  es_rivendell_15_gp,
  es_ocean_breeze_036_gp,
  rgi_15_gp,
  retro2_16_gp,
  Analogous_1_gp,
  es_pinksplash_08_gp,
  Coral_reef_gp,
  es_ocean_breeze_068_gp,
  es_pinksplash_07_gp,
  es_vintage_01_gp, //10
  departure_gp,
  es_landscape_64_gp,
  es_landscape_33_gp,
  rainbowsherbet_gp,
  gr65_hult_gp,
  gr64_hult_gp,
  GMT_drywet_gp,
  ib_jul01_gp,
  es_vintage_57_gp,
  ib15_gp,  // 20
  Fuschia_7_gp,
  es_emerald_dragon_08_gp,
  lava_gp,
  fire_gp,
  Colorfull_gp,
  Magenta_Evening_gp,
  Pink_Purple_gp,
  es_autumn_19_gp,
  BlacK_Blue_Magenta_White_gp,
  BlacK_Magenta_Red_gp, // 30
  BlacK_Red_Magenta_Yellow_gp,
  Blue_Cyan_Yellow_gp,
  Trans_Flag_gp,
  Enby_Flag_gp,
  Genderqueer_Flag_gp,
  Pride_Flag_gp,
  Bi_Flag_gp,
  Lesbian_Flag_gp,
  Pan_Flag_gp,
};

const uint8_t gGradientPaletteCount =
  sizeof( gGradientPalettes) / sizeof( TProgmemRGBGradientPaletteRef );

/* --- */

template <class T>
class PaletteManager {
private:
  bool paletteHasColorBelowThreshold(T &palette, uint8_t minBrightness) {
    if (minBrightness == 0) {
      return false;
    }
    for (uint16_t i = 0; i < sizeof(T)/3; ++i) {
      if (palette.entries[i].getAverageLight() < minBrightness) {
        return true;
      }
    }
    return false;
  }

  uint8_t paletteColorJump(T& palette, bool wrapped=false) {
    uint8_t maxJump = 0;
    CRGB lastColor = palette.entries[(wrapped ? sizeof(T)/3 - 1 : 1)];
    for (uint16_t i = (wrapped ? 0 : 1); i < sizeof(T)/3; ++i) {
      CRGB color = palette.entries[i];
      uint8_t distance = (abs((int)color.r - (int)lastColor.r) + abs((int)color.g - (int)lastColor.g) + abs((int)color.b - (int)lastColor.b)) / 3;
      if (distance > maxJump) {
        maxJump = distance;
      }
      lastColor = color;
    }
    return maxJump;
  }
public:  
  PaletteManager() {
  }

  T getPalette(int choice) {
    return gGradientPalettes[choice];
  }
  
  void getRandomPalette(T* palettePtr, uint8_t minBrightness=0, uint8_t maxColorJump=0xFF) {
    if (!palettePtr) return;
    uint8_t choices[gGradientPaletteCount];
    for (int i = 0; i < gGradientPaletteCount; ++i) {
      choices[i] = i;
    }
    shuffle<uint8_t, gGradientPaletteCount>(choices);
    for (int i = 0; i < gGradientPaletteCount; ++i) {
      *palettePtr = gGradientPalettes[choices[i]];
      bool belowMinBrightness = paletteHasColorBelowThreshold(*palettePtr, minBrightness);
      uint8_t colorJump = paletteColorJump(*palettePtr);
      if (!belowMinBrightness && colorJump <= maxColorJump) {
        logf("  Picked Palette %u", choices[i]);
        return;
      }
    }
    
    logf("Giving up choosing an acceptable palette; minBrightness=%i, maxColorJump=%i", minBrightness, maxColorJump);
    *palettePtr = gGradientPalettes[random16(gGradientPaletteCount)];
  }
};


/* -------------------------------------------------------------------- */

template<typename PaletteType>
void nblendPaletteTowardPalette(PaletteType& current, PaletteType& target, uint16_t maxChanges)
{
  uint8_t* p1;
  uint8_t* p2;
  uint16_t  changes = 0;

  p1 = (uint8_t*)current.entries;
  p2 = (uint8_t*)target.entries;

  const uint16_t totalChannels = sizeof(PaletteType);
  for( uint16_t i = 0; i < totalChannels; i++) {
    // if the values are equal, no changes are needed
    if( p1[i] == p2[i] ) { continue; }

    // if the current value is less than the target, increase it by one
    if( p1[i] < p2[i] ) { p1[i]++; changes++; }

    // if the current value is greater than the target,
    // increase it by one (or two if it's still greater).
    if( p1[i] > p2[i] ) {
        p1[i]--; changes++;
        if( p1[i] > p2[i] ) { p1[i]--; }
    }

    // if we've hit the maximum number of changes, exit
    if( changes >= maxChanges) { break; }
  }
}

template <typename PaletteType>
class PaletteRotation {
private:
  PaletteManager<PaletteType> manager;
  PaletteType currentPalette;
  PaletteType targetPalette;

  void assignPalette(PaletteType* palettePtr) {
    manager.getRandomPalette(palettePtr, minBrightness, maxColorJump);
  }
  unsigned long lastBlendStep = 0;
  unsigned long lastPaletteChange = 0;
public:
  unsigned int secondsPerPalette = 10;
  uint8_t minBrightness = 0;
  uint8_t maxColorJump = 0xFF;
  bool pauseRotation = false;
  
  PaletteRotation(int minBrightness=0) : minBrightness(minBrightness) { }
  
  void paletteRotationTick() {
    if (lastPaletteChange == 0) {
      assignPalette(&currentPalette);
      assignPalette(&targetPalette);
      lastPaletteChange = millis();
    }
    if (!pauseRotation) {
      if (millis() - lastBlendStep > 40) {
        nblendPaletteTowardPalette<PaletteType>(currentPalette, targetPalette, sizeof(PaletteType) / 3);
        lastBlendStep = millis();
      }
      if (millis() - lastPaletteChange > 1000 * secondsPerPalette) {
        assignPalette(&targetPalette);
        lastPaletteChange = millis();
      }
    }
  }

  PaletteType& getPalette() {
    paletteRotationTick();
    return currentPalette;
  }

  // unblended override
  virtual void setPalette(PaletteType palette) {
    currentPalette = palette;
    if (lastPaletteChange == 0) {
      assignPalette(&targetPalette);
    }
    lastPaletteChange = millis();
  }

  void randomizePalette() {
    assignPalette(&currentPalette);
  }

  inline CRGB getPaletteColor(PaletteType& palette, uint8_t n, uint8_t brightness=0xFF) {
    return ColorFromPalette(palette, n, brightness);
  }

  inline CRGB getPaletteColor(uint8_t n, uint8_t brightness = 0xFF) {
    return getPaletteColor(getPalette(), n, brightness);
  }

  CRGB getLumaNormalizedPaletteColor(PaletteType& palette, uint8_t n, uint8_t luma) {
    CRGB color = getPaletteColor(palette, n);
    uint8_t oldLuma = color.getLuma();
    CRGB normalized;
    normalized.r = 0xFF * color.r * luma / oldLuma / 0xFF;
    normalized.g = 0xFF * color.g * luma / oldLuma / 0xFF;
    normalized.b = 0xFF * color.b * luma / oldLuma / 0xFF;
    return normalized;
  }

  CRGB getMaxLumaPaletteColor(PaletteType& palette) {
    int maxLuma = 0;
    CRGB color = CRGB::Black;
    for (int i = 0; i < sizeof(palette.entries) / sizeof(palette.entries[0]); ++i) {
      auto luma = palette[i].getLuma();
      if (luma > maxLuma){
        maxLuma = luma;
        color = palette[i];
      }
    }
    return color;
  }

  inline CRGB getLumaNormalizedPaletteColor(uint8_t n, uint8_t luma) {
    return getLumaNormalizedPaletteColor(getPalette(), n, luma);
  }

  CRGB getMirroredPaletteColor(PaletteType& palette, uint16_t n, uint8_t brightness = 0xFF, uint8_t *outColorIndex=NULL) {
    n = n % 0x200;
    if (n >= 0x100) {
      n = 0x200 - n - 1;
    }
    if (outColorIndex) {
      *outColorIndex = n;
    }
    return ColorFromPalette(palette, n, brightness);
  }

  inline CRGB getMirroredPaletteColor(uint16_t n, uint8_t brightness = 0xFF, uint8_t *outColorIndex=NULL) {
    return getMirroredPaletteColor(getPalette(), n, brightness, outColorIndex);
  }

  CRGB getShiftingPaletteColor(uint16_t phase, int speed=2/*cycles per minute*/, uint8_t brightness = 0xFF, bool mirrored=true) {
    PaletteType& palette = getPalette();
    uint16_t index = phase + 0xFF * speed * millis() / 1000 / 60;
    return (mirrored ? getMirroredPaletteColor(palette, index, brightness) : getPaletteColor(palette, index, brightness));
  }
};

#endif
