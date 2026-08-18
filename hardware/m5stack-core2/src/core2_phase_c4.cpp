#include <Arduino.h>
#include <ArduinoJson.h>
#include <M5Unified.h>

#include "statelamp_model.h"
#include "ble_gatt_adapter.h"
#include "core2_audio.h"
#include "core2_ui.h"

#if !defined(CORE2_PHASE_C4)
#error "core2_phase_c4.cpp is only for the Core2 Phase C4 target"
#endif

namespace {
constexpr uint32_t kHeartbeatIntervalMs = 5000;
StateLampStateMachine StateLamp;
uint32_t lastHeartbeatAt = 0;

String commandMessage(const char* payload, size_t length) {
  JsonDocument document;
  if (deserializeJson(document, payload, length)) return String();
  if (!document["message"].is<const char*>()) return String();
  return String(document["message"].as<const char*>());
}

bool handleCommand(void* context, const char* payload, size_t length,
                   String& response) {
  auto& stateMachine = *static_cast<StateLampStateMachine*>(context);
  const StateLampState previous = stateMachine.state();
  const bool accepted = stateMachine.applyJson(payload, length, response);
  if (accepted) {
    const String message = commandMessage(payload, length);
    const StateLampState current = stateMachine.state();
    core2_ui::render(current, message);
    if (current != previous) core2_audio::onStateTransition(current, millis());
    Serial.printf(
        "core2-c4: display state=%s, message_bytes=%u, transitioned=%s\n",
        stateMachine.stateName(), message.length(),
        current != previous ? "yes" : "no");
  }
  return accepted;
}

BleGattAdapter ble(&StateLamp, handleCommand, "core2-c4");
}  // namespace

void setup() {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.clear_display = false;
  config.output_power = false;
  config.internal_imu = false;
  config.internal_rtc = false;
  config.internal_spk = true;
  config.internal_mic = false;
  M5.begin(config);

  Serial.println();
  Serial.println("StateLamp Core2 Phase C4");
  Serial.println("scope: C3 UI + transition-only generated notification audio");
  StateLamp.begin();
  core2_ui::begin();
  core2_ui::render(StateLamp.state());
  core2_audio::begin();
  ble.begin("StateLamp");
  lastHeartbeatAt = millis();
  Serial.println("core2-c4: ready (no startup tone)");
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  core2_ui::tick(now);
  core2_audio::tick(now);
  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf(
        "core2-c4: alive, state=%s, audio=%s, uptime_ms=%u, free_heap=%u\n",
        StateLamp.stateName(), core2_audio::isActive() ? "active" : "idle", now,
        ESP.getFreeHeap());
  }
  delay(10);
}
