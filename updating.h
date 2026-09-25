#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include <Arduino.h>
#include <stddef.h>
#include "util.h"

#include <string.h>
#include "pico/unique_id.h"

const char* kIdentifyCommand = "IDENTIFY";
const char* kBlinkCommand = "BLINK";

// Serial responder for the Newer Glow updater (github.com/starduststorm/newerglow).
//
// IDENTIFY -> "ID:<product> v<fw_version> hw=<hw_revision> sn=<unique board id>"
//   product      the newer-glow board id, which is also the GitHub repo name and the release asset prefix
//   fw_version   FW_VERSION from scripts/fw_version.py (the fw-v<version> release tag, never hand-bumped)
//   hw_revision  the board revision as silkscreened ("5", "8", "mini2"), not the build env: a build that covers
//                several revisions must detect which one it's running on. The updater fetches the release asset
//                <product>-<fw_version>-hw<hw_revision>.uf2, so this must match a custom_release_hw entry in
//                platformio.ini exactly.
// BLINK    -> blinkFunc(), a short LED flash so the user can tell attached devices apart.
class NewerGlowUpdater {
public:
  using BlinkFunc = std::function<void(void)>;

  NewerGlowUpdater(
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
