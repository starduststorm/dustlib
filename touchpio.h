#pragma once

// Capacitive touch pads on RP2040/RP2350 using one PIO state machine for up to five
// consecutive GPIOs, with no external parts beyond the pads themselves.
// Based on https://github.com/forshee9283/pio-touch
//
// The state machine charges the pads, floats them, and samples them a fixed number of
// PIO cycles later against the internal pulldowns; a touched pad's extra capacitance
// still reads high. Changes are debounced in the PIO and pushed to the RX FIFO, which
// an interrupt drains into state().
//
//   TouchPIO touch;
//   touch.begin(/*firstPin*/ 0, /*pinCount*/ 5);
//   controls.addControl(new TouchButton(touch, 2));
//
// PIO resources are scarce (4 SMs and 32 instruction words per PIO, and this program
// takes 20 of those words), so projects that also run pixels, UARTs, PDM, etc. may need
// to pin placement via TouchPIO::Options rather than take whichever PIO has room.

#if defined(ARDUINO_ARCH_RP2040)

#include "hardware/pio.h"
#include "hardware/irq.h"
#include "util.h"
#include "controls.h"

class TouchPIO {
public:
  static constexpr unsigned kMaxPins = 5; // PIO SET instructions carry 5 bits of data
  static constexpr unsigned kProgramLength = 20;

  struct Options {
    // PIO block to use, or nullptr for the first one with program space and a free SM.
    PIO pio = nullptr;
    // Which of the PIO's two interrupt lines (PIOx_IRQ_0 or PIOx_IRQ_1) to take, exclusively.
    // Defaults to 1 because arduino-pico's SerialPIO takes line 0 of any PIO it lands on.
    uint8_t irqLine = 1;
    // Sample timing, i.e. sensitivity; larger pads or thicker overlays want a larger divider.
    float clkDiv = 40;
  };

private:
  // pioasm output for the program below; `in` widths are patched per pin count in program().
  //
  //   debounce_setup:
  //       pull noblock              ; empty TX FIFO, so this refills osr (from x) as a 32-tick counter
  //       mov y, x
  //   debounce_loop:
  //       out null, 1               ; tick down osr counter
  //       in null, 27               ; pre-pad isr so we don't get leftover data
  //       set pindirs, 31           ; all pads to outputs
  //       set pins, 31              ; charge
  //       set pindirs, 0 [2]        ; float them
  //       in pins, 5 [6]            ; sample
  //       mov x, isr
  //       jmp x!=y, debounce_setup  ; input changed during debounce, restart the count
  //       jmp !osre, debounce_loop
  //       push block
  //   .wrap_target
  //       mov y, x
  //       in null, 27
  //       set pindirs, 31
  //       set pins, 31
  //       set pindirs, 0 [2]
  //       in pins, 5 [6]
  //       mov x, isr
  //       jmp x!=y, debounce_setup  ; input changed, go debounce it
  //   .wrap
  static constexpr uint16_t kProgram[kProgramLength] = {
    0x8080, 0xa041, 0x6061, 0x407b, 0xe09f, 0xe01f, 0xe280, 0x4605, 0xa026, 0x00a0, 0x00e2, 0x8020,
    0xa041, 0x407b, 0xe09f, 0xe01f, 0xe280, 0x4605, 0xa026, 0x00a0,
  };
  static constexpr unsigned kWrapTarget = 12, kWrap = 19;

  // One program per pin count, since the sample width is baked into the instructions. Sampling
  // all five pins regardless would let unrelated activity on the GPIOs past pinCount read as
  // pad changes and hold the state machine in its debounce loop.
  struct Program {
    uint16_t instructions[kProgramLength];
    pio_program_t program;
    int offset[NUM_PIOS]; // where it's loaded in each PIO, -1 if not
  };
  static Program &program(unsigned pinCount) {
    static Program programs[kMaxPins];
    Program &p = programs[pinCount - 1];
    if (p.program.length == 0) {
      memcpy(p.instructions, kProgram, sizeof(kProgram));
      p.instructions[3] = p.instructions[13] = 0x4060 | (32 - pinCount); // in null, 32-pinCount
      p.instructions[7] = p.instructions[17] = 0x4600 | pinCount;        // in pins, pinCount [6]
      p.program = { .instructions = p.instructions, .length = kProgramLength, .origin = -1 };
      for (int &offset : p.offset) {
        offset = -1;
      }
    }
    return p;
  }

  // irq handlers take no context, so every instance registers here and each handler call
  // services all of them; an empty FIFO check is cheap.
  static constexpr unsigned kMaxInstances = NUM_PIOS * 4;
  inline static TouchPIO *instances[kMaxInstances] = {0};
  inline static uint32_t irqsInstalled = 0; // bit per (pio index * 2 + irqLine)

  static void irqHandler() {
    for (TouchPIO *touch : instances) {
      if (touch) {
        touch->drain();
      }
    }
  }

  void drain() {
    while (!pio_sm_is_rx_fifo_empty(_pio, _sm)) {
      _state = pio_sm_get(_pio, _sm) & ((1u << pinCount) - 1);
    }
  }

  PIO _pio = nullptr;
  int _sm = -1;
  unsigned pinCount = 0;
  volatile uint32_t _state = 0;

public:
  // Starts sensing pads on GPIOs [firstPin, firstPin + pinCount). Returns false, claiming nothing,
  // if the requested (or any) PIO lacks a free state machine or room for the program.
  bool begin(unsigned firstPin, unsigned pinCount, Options options) {
    assert(!_pio, "TouchPIO: begin called twice");
    assert(pinCount >= 1 && pinCount <= kMaxPins, "TouchPIO: 1-%u pins per state machine", kMaxPins);
    assert(options.irqLine <= 1, "TouchPIO: irqLine is 0 or 1");
    if (_pio || pinCount < 1 || pinCount > kMaxPins || options.irqLine > 1) {
      return false;
    }
    Program &prog = program(pinCount);

    PIO pios[] = { pio0, pio1,
#if NUM_PIOS > 2
      pio2,
#endif
    };
    PIO pio = nullptr;
    int sm = -1;
    for (PIO p : pios) {
      if (options.pio && options.pio != p) {
        continue;
      }
      if (prog.offset[pio_get_index(p)] >= 0 || pio_can_add_program(p, &prog.program)) {
        sm = pio_claim_unused_sm(p, false);
        if (sm >= 0) {
          pio = p;
          break;
        }
      }
    }
    TouchPIO **slot = nullptr;
    for (TouchPIO *&instance : instances) {
      if (!instance) {
        slot = &instance;
        break;
      }
    }
    if (!pio || !slot) {
      logf("TouchPIO: no free PIO state machine with %u instruction words available", kProgramLength);
      return false;
    }
    const unsigned pioIndex = pio_get_index(pio);
    if (prog.offset[pioIndex] < 0) {
      prog.offset[pioIndex] = pio_add_program(pio, &prog.program);
    }
    const unsigned offset = prog.offset[pioIndex];

    for (unsigned i = 0; i < pinCount; ++i) {
      pio_gpio_init(pio, firstPin + i);
      gpio_pull_down(firstPin + i);
    }
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + kWrapTarget, offset + kWrap);
    sm_config_set_clkdiv(&c, options.clkDiv);
    sm_config_set_set_pins(&c, firstPin, pinCount);
    sm_config_set_in_pins(&c, firstPin);
    sm_config_set_in_shift(&c, false, false, 32);
    pio_sm_init(pio, sm, offset, &c);

    _pio = pio;
    _sm = sm;
    this->pinCount = pinCount;
    *slot = this;

    pio_set_irqn_source_enabled(pio, options.irqLine, pio_get_rx_fifo_not_empty_interrupt_source(sm), true);
    const unsigned irqBit = pioIndex * 2 + options.irqLine;
    if (!(irqsInstalled & (1u << irqBit))) {
      const unsigned irq = pio_get_irq_num(pio, options.irqLine);
      irq_set_exclusive_handler(irq, irqHandler);
      irq_set_enabled(irq, true);
      irqsInstalled |= 1u << irqBit;
    }
    pio_sm_set_enabled(pio, sm, true);
    return true;
  }

  bool begin(unsigned firstPin, unsigned pinCount) {
    return begin(firstPin, pinCount, Options());
  }

  // Bitfield of touched pads; bit i is GPIO firstPin + i.
  uint32_t state() const { return _state; }
  bool isTouched(unsigned index) const { return _state & (1u << index); }

  // Where this landed, for logging or verifying a project's PIO layout. nullptr/-1 before begin.
  PIO pio() const { return _pio; }
  int sm() const { return _sm; }
};

#endif // ARDUINO_ARCH_RP2040
