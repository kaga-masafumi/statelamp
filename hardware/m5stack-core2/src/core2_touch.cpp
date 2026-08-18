#include "core2_touch.h"

#include <Arduino.h>
#include <M5Unified.h>

#if !defined(CORE2_PHASE_C5) && !defined(CORE2_WIFI)
#error "core2_touch.cpp is only for the Core2 Phase C5 target"
#endif

namespace core2_touch {
namespace {
constexpr uint32_t kMuteHoldMs = 800;
}

void begin() {
  M5.BtnB.setHoldThresh(kMuteHoldMs);
  M5.BtnA.setHoldThresh(kMuteHoldMs);
  Serial.printf(
      "core2-c5: touch ready, action=rotation_lock, button=A; "
      "action=local_audio_toggle, button=B, hold_ms=%u\n",
      kMuteHoldMs);
}

bool muteToggleRequested() {
  if (!M5.BtnB.wasHold()) return false;
  Serial.printf("core2-c5: local mute toggle requested, uptime_ms=%u\n",
                millis());
  return true;
}

bool rotationLockToggleRequested() {
  if (!M5.BtnA.wasHold()) return false;
  Serial.printf("core2-c5: local rotation lock toggle requested, uptime_ms=%u\n",
                millis());
  return true;
}

}  // namespace core2_touch
