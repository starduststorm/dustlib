#ifndef APA102PIO_H
#define APA102PIO_H

// PIO+DMA transport for APA102/SK9822-class pixels on RP2040/RP2350.
//
// FastLED has no hardware SPI backend for rp2xxx, so its SPI chipsets fall back to
// software bitbang. This controller renders the same bytes as SK9822ControllerHD
// (via FastLED PixelController::loadAndScale_APA102_HD, so gamma, global brightness,
// and dithering behavior are identical) into a RAM frame buffer, then hands the
// buffer to a DMA channel feeding a 2-instruction PIO clock+data program. 

#if defined(ARDUINO_ARCH_RP2040)

#include <FastLED.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "util.h"

template<EOrder RGB_ORDER = RGB, uint32_t END_FRAME = 0x00000000>
class APA102PIOController : public CPixelLEDController<RGB_ORDER> {
  const uint8_t dataPin, clockPin;
  const uint32_t bitrateHz;
  PIO pio = nullptr;
  int sm = -1;
  int dmaChan = -1;
  uint32_t *frameBuf = nullptr;
  int frameWords = 0;

  // clocked serial out, 2 PIO cycles per bit, clock idles low, data valid before rising edge:
  //   out pins, 1  side 0
  //   nop          side 1
  static constexpr uint16_t kProgram[2] = {0x6001, 0xB042};

  void setup(int nLeds) {
    frameWords = 1 + nLeds + (nLeds/32 + 1); // start frame + pixels + end frame
    frameBuf = (uint32_t *)malloc(frameWords * sizeof(uint32_t));

    static const struct pio_program prog = { kProgram, 2, -1 };
    PIO pios[] = { pio0, pio1,
#if NUM_PIOS > 2
      pio2,
#endif
    };
    for (PIO p : pios) {
      if (pio_can_add_program(p, &prog)) {
        sm = pio_claim_unused_sm(p, false);
        if (sm >= 0) {
          pio = p;
          break;
        }
      }
    }
    assert(pio && frameBuf, "APA102PIOController: no free PIO state machine");
    uint offset = pio_add_program(pio, &prog);

    pio_gpio_init(pio, dataPin);
    pio_gpio_init(pio, clockPin);
    pio_sm_set_consecutive_pindirs(pio, sm, dataPin, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, clockPin, 1, true);
    pio_sm_set_pins_with_mask(pio, sm, 0, (1u << dataPin) | (1u << clockPin));

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 1);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, clockPin);
    sm_config_set_out_pins(&c, dataPin, 1);
    sm_config_set_out_shift(&c, false /*shift left: MSB first*/, true /*autopull*/, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (2.0f * bitrateHz));
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    dmaChan = dma_claim_unused_channel(true);
    dma_channel_config dc = dma_channel_get_default_config(dmaChan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dmaChan, &dc, &pio->txf[sm], NULL, frameWords, false);
  }

public:
  APA102PIOController(uint8_t dataPin, uint8_t clockPin, uint32_t bitrateHz)
    : dataPin(dataPin), clockPin(clockPin), bitrateHz(bitrateHz) { }

  virtual void init() override { }

protected:
  virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
    if (!pio) {
      setup(pixels.size());
    }
    // the previous frame's transfer reads frameBuf; normally long done by now
    dma_channel_wait_for_finish_blocking(dmaChan);

    uint32_t *w = frameBuf;
    *w++ = 0x00000000; // start frame
    while (pixels.has(1)) {
      uint8_t brightness, c0, c1, c2;
      pixels.loadAndScale_APA102_HD(&c0, &c1, &c2, &brightness);
      *w++ = ((uint32_t)(0xE0 | brightness) << 24) | ((uint32_t)c0 << 16) | ((uint32_t)c1 << 8) | c2;
      pixels.stepDithering();
      pixels.advanceData();
    }
    while (w < frameBuf + frameWords) {
      *w++ = END_FRAME;
    }
    dma_channel_set_read_addr(dmaChan, frameBuf, false);
    dma_channel_set_trans_count(dmaChan, frameWords, true /*trigger*/);
  }
};

#endif // ARDUINO_ARCH_RP2040
#endif // APA102PIO_H
