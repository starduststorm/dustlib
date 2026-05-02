#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include <Arduino.h>
#include <stddef.h>
#include "util.h"

#include <string.h>
#include "pico/unique_id.h"

const char* kIdentifyCommand = "IDENTIFY";
const char* kBlinkCommand = "BLINK";

class RP2040Updater {
public:
  using BlinkFunc = std::function<void(void)>;

  RP2040Updater(
            const char* deviceName,
            const char* firmwareVersion,
            const char* hardwareVersion,
            BlinkFunc blinkFunc
          ) : 
          deviceName(deviceName), 
          firmwareVersion(firmwareVersion), 
          hardwareVersion(hardwareVersion), 
          blinkFunc(blinkFunc) { }

  void loop(char *line) {
    if (line) {
      if (strcmp(line, kIdentifyCommand) == 0) {
        respondIdentify();
      } else if (strcmp(line, kBlinkCommand) == 0) {
        if (blinkFunc){ 
          blinkFunc();
        }
      }
    }
  }
private:

  const char* deviceName      = nullptr;
  const char* firmwareVersion = nullptr;
  const char* hardwareVersion = nullptr;
  BlinkFunc blinkFunc         = nullptr;

  void respondIdentify() {
    logf("ID:%s v%s hw=%s sn=%s", (deviceName ?: "Unknown"), (firmwareVersion ?: "0.0.0"), (hardwareVersion ?: "0"), boardIdHex());
    Serial.flush();
  }

  const char* boardIdHex() {
    static char id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    static bool inited = false;
    if (!inited) {
      pico_get_unique_board_id_string(id, sizeof(id));
      inited = true;
    }
    return id;
  }
};

#endif
