#include <Arduino.h>
#include <M5Unified.h>

#if !defined(CORE2_PHASE_C5_DIAGNOSTIC)
#error "core2_touch_diagnostic.cpp is only for the Core2 C5 diagnostic target"
#endif

namespace {
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr uint32_t kMoveLogIntervalMs = 80;
constexpr int16_t kButtonBoundaryY = 240;

uint32_t lastHeartbeatAt = 0;
uint32_t lastMoveLogAt = 0;
int16_t lastX = -1;
int16_t lastY = -1;

const char* stateName(m5::touch_state_t state) {
  switch (state) {
    case m5::touch_state_t::none: return "none";
    case m5::touch_state_t::touch: return "touch";
    case m5::touch_state_t::touch_begin: return "touch_begin";
    case m5::touch_state_t::touch_end: return "touch_end";
    case m5::touch_state_t::hold: return "hold";
    case m5::touch_state_t::hold_begin: return "hold_begin";
    case m5::touch_state_t::hold_end: return "hold_end";
    case m5::touch_state_t::flick: return "flick";
    case m5::touch_state_t::flick_begin: return "flick_begin";
    case m5::touch_state_t::flick_end: return "flick_end";
    case m5::touch_state_t::drag: return "drag";
    case m5::touch_state_t::drag_begin: return "drag_begin";
    case m5::touch_state_t::drag_end: return "drag_end";
  }
  return "unknown";
}

const char* regionName(int16_t x, int16_t y) {
  if (y < kButtonBoundaryY) return "display";
  if (x < 107) return "button_a";
  if (x < 213) return "button_b";
  return "button_c";
}

void drawBase() {
  M5.Display.setRotation(1);
  M5.Display.fillScreen(0x101820U);
  M5.Display.setTextColor(0xFFFFFFU, 0x101820U);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(18, 18);
  M5.Display.println("CORE2 C5 TOUCH DIAGNOSTIC");
  M5.Display.setTextSize(1);
  M5.Display.setCursor(18, 50);
  M5.Display.println("No actions are bound. State is not mutated.");
  M5.Display.drawRect(8, 72, 304, 112, 0x607080U);
  M5.Display.setCursor(18, 196);
  M5.Display.println("Touch display, drag, hold, then press A / B / C.");
  M5.Display.setCursor(18, 216);
  M5.Display.println("Coordinates and events are written to Serial.");
}

void drawTouch(const m5::touch_detail_t& detail) {
  M5.Display.fillRect(9, 73, 302, 110, 0x101820U);
  M5.Display.setTextColor(0xFFFFFFU, 0x101820U);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(24, 88);
  M5.Display.printf("x=%d  y=%d\n", detail.x, detail.y);
  M5.Display.setCursor(24, 116);
  M5.Display.printf("event: %s\n", stateName(detail.state));
  M5.Display.setCursor(24, 144);
  M5.Display.printf("region: %s", regionName(detail.x, detail.y));
}

void logButtons() {
  if (M5.BtnA.wasPressed() || M5.BtnA.wasReleased()) {
    Serial.printf("core2-c5-touch: virtual_button=A event=%s uptime_ms=%u\n",
                  M5.BtnA.isPressed() ? "pressed" : "released", millis());
  }
  if (M5.BtnB.wasPressed() || M5.BtnB.wasReleased()) {
    Serial.printf("core2-c5-touch: virtual_button=B event=%s uptime_ms=%u\n",
                  M5.BtnB.isPressed() ? "pressed" : "released", millis());
  }
  if (M5.BtnC.wasPressed() || M5.BtnC.wasReleased()) {
    Serial.printf("core2-c5-touch: virtual_button=C event=%s uptime_ms=%u\n",
                  M5.BtnC.isPressed() ? "pressed" : "released", millis());
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
  Serial.println("StateLamp Core2 Phase C5 touch diagnostic");
  Serial.println("scope: observation only; no BLE, audio, or state mutation");
  Serial.printf("core2-c5-touch: enabled=%s, display=%dx%d, button_y>=%d\n",
                M5.Touch.isEnabled() ? "yes" : "no", M5.Display.width(),
                M5.Display.height(), kButtonBoundaryY);
  drawBase();
  lastHeartbeatAt = millis();
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  logButtons();

  if (M5.Touch.getCount()) {
    const auto& detail = M5.Touch.getDetail(0);
    const bool changed = detail.x != lastX || detail.y != lastY;
    const bool edge = detail.wasPressed() || detail.wasReleased() ||
                      detail.wasHold() || detail.wasFlickStart() ||
                      detail.wasFlicked() || detail.wasDragStart() ||
                      detail.wasDragged();
    if (edge || (changed && now - lastMoveLogAt >= kMoveLogIntervalMs)) {
      lastMoveLogAt = now;
      lastX = detail.x;
      lastY = detail.y;
      drawTouch(detail);
      Serial.printf(
          "core2-c5-touch: event=%s x=%d y=%d base=(%d,%d) delta=(%d,%d) "
          "region=%s count=%u uptime_ms=%u\n",
          stateName(detail.state), detail.x, detail.y, detail.base_x,
          detail.base_y, detail.deltaX(), detail.deltaY(),
          regionName(detail.x, detail.y), M5.Touch.getCount(), now);
    }
  }

  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf("core2-c5-touch: alive, free_heap=%u, uptime_ms=%u\n",
                  ESP.getFreeHeap(), now);
  }
  delay(10);
}
