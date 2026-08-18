#include <Arduino.h>
#include <M5Unified.h>

#include "core2_display_diagnostic.h"

#if !defined(CORE2_PHASE_C1)
#error "core2_phase_c1.cpp is only for the Core2 Phase C1 target"
#endif

namespace {
constexpr uint32_t kHeartbeatIntervalMs = 5000;
Core2DisplayDiagnostic displayDiagnostic;
uint32_t lastHeartbeatAt = 0;
}  // namespace

void setup() {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.clear_display = false;
  config.output_power = false;
  config.internal_imu = false;
  config.internal_rtc = false;
  config.internal_spk = false;
  config.internal_mic = false;
  M5.begin(config);

  displayDiagnostic.begin();

  Serial.println();
  Serial.println("StateLamp Core2 Phase C1");
  Serial.println("scope: M5Unified/M5GFX LCD diagnostic only");
  Serial.printf("display=%dx%d rotation=%u touch_enabled=%s\n",
                M5.Display.width(), M5.Display.height(),
                M5.Display.getRotation(),
                M5.Touch.isEnabled() ? "yes" : "no");
  Serial.println("sequence: RED GREEN BLUE BLACK WHITE; 2s plain + 3s label");
  Serial.printf("phase-c1: color=%s view=plain\n",
                displayDiagnostic.colorName());
  Serial.println("phase-c1: ready");
  lastHeartbeatAt = millis();
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  displayDiagnostic.update(now);
  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf("phase-c1: alive, uptime_ms=%u, free_heap=%u, color=%s, view=%s\n",
                  now, ESP.getFreeHeap(), displayDiagnostic.colorName(),
                  displayDiagnostic.isLabelVisible() ? "label" : "plain");
  }
  delay(10);
}
