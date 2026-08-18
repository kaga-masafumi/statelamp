#include <Arduino.h>
#include <M5Unified.h>

#include "statelamp_model.h"
#include "ble_gatt_adapter.h"
#include "core2_state_display.h"

#if !defined(CORE2_PHASE_C2)
#error "core2_phase_c2.cpp is only for the Core2 Phase C2 target"
#endif

namespace {
constexpr uint32_t kHeartbeatIntervalMs = 5000;
StateLampStateMachine StateLamp;
uint32_t lastHeartbeatAt = 0;

bool handleCommand(void* context, const char* payload, size_t length,
                   String& response) {
  auto& stateMachine = *static_cast<StateLampStateMachine*>(context);
  const bool accepted = stateMachine.applyJson(payload, length, response);
  if (accepted) {
    core2_state_display::render(stateMachine.state());
    Serial.printf("core2-c2: display state=%s\n", stateMachine.stateName());
  }
  return accepted;
}

BleGattAdapter ble(&StateLamp, handleCommand, "core2-c2");
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

  Serial.println();
  Serial.println("StateLamp Core2 Phase C2");
  Serial.println("scope: shared BLE GATT + state machine + full-screen colors");
  StateLamp.begin();
  core2_state_display::begin();
  core2_state_display::render(StateLamp.state());
  ble.begin("StateLamp");
  lastHeartbeatAt = millis();
  Serial.println("core2-c2: ready");
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf("core2-c2: alive, state=%s, uptime_ms=%u, free_heap=%u\n",
                  StateLamp.stateName(), now, ESP.getFreeHeap());
  }
  delay(10);
}
