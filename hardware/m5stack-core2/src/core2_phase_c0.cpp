#include <Arduino.h>
#include <M5Unified.h>

#if !defined(CORE2_PHASE_C0)
#error "core2_phase_c0.cpp is only for the Core2 Phase C0 target"
#endif

namespace {
constexpr uint32_t kHeartbeatIntervalMs = 5000;
uint32_t lastHeartbeatAt = 0;

const char* boardName(m5::board_t board) {
  switch (board) {
    case m5::board_t::board_M5StackCore2: return "M5Stack Core2";
    default: return "unexpected";
  }
}

const char* pmicName(m5::Power_Class::pmic_t pmic) {
  switch (pmic) {
    case m5::Power_Class::pmic_axp192: return "AXP192";
    case m5::Power_Class::pmic_axp2101: return "AXP2101";
    default: return "unexpected";
  }
}

const char* core2Revision(m5::Power_Class::pmic_t pmic) {
  switch (pmic) {
    case m5::Power_Class::pmic_axp192: return "K010 (original Core2)";
    case m5::Power_Class::pmic_axp2101: return "K010-V11 (Core2 v1.1)";
    default: return "unknown";
  }
}
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
  Serial.println("StateLamp Core2 Phase C0");
  Serial.println("scope: M5Unified boot + Serial hardware inventory only");
  Serial.printf("board=%s (%d)\n", boardName(M5.getBoard()),
                static_cast<int>(M5.getBoard()));
  const auto pmic = M5.Power.getType();
  Serial.printf("pmic=%s (%d) inferred_product=%s\n", pmicName(pmic),
                static_cast<int>(pmic), core2Revision(pmic));
  Serial.printf("chip=%s revision=%u cores=%u cpu_mhz=%u\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                ESP.getCpuFreqMHz());
  Serial.printf("flash_bytes=%u psram_bytes=%u free_heap=%u\n",
                ESP.getFlashChipSize(), ESP.getPsramSize(), ESP.getFreeHeap());
  Serial.printf("display=%dx%d rotation=%u touch_enabled=%s\n",
                M5.Display.width(), M5.Display.height(), M5.Display.getRotation(),
                M5.Touch.isEnabled() ? "yes" : "no");
  Serial.printf("speaker_enabled=%s microphone_enabled=%s\n",
                M5.Speaker.isEnabled() ? "yes" : "no",
                M5.Mic.isEnabled() ? "yes" : "no");
  Serial.println("phase-c0: ready");
  lastHeartbeatAt = millis();
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf("phase-c0: alive, uptime_ms=%u, free_heap=%u\n", now,
                  ESP.getFreeHeap());
  }
  delay(10);
}
