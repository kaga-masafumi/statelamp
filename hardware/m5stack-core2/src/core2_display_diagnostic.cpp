#include "core2_display_diagnostic.h"

#if !defined(CORE2_PHASE_C1)
#error "core2_display_diagnostic.cpp is only for the Core2 Phase C1 target"
#endif

namespace {
struct TestColor {
  uint32_t value;
  uint32_t text;
  const char* name;
};

constexpr TestColor kColors[] = {
    {0xFF0000U, 0xFFFFFFU, "RED"},
    {0x00FF00U, 0x000000U, "GREEN"},
    {0x0000FFU, 0xFFFFFFU, "BLUE"},
    {0x000000U, 0xFFFFFFU, "BLACK"},
    {0xFFFFFFU, 0x000000U, "WHITE"},
};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);
}  // namespace

void Core2DisplayDiagnostic::begin() {
  M5.Display.setRotation(kRotation);
  colorIndex_ = 0;
  labelVisible_ = false;
  changedAt_ = millis();
  drawPlain();
}

void Core2DisplayDiagnostic::update(uint32_t now) {
  const uint32_t duration =
      labelVisible_ ? kLabelDurationMs : kPlainDurationMs;
  if (now - changedAt_ < duration) return;

  if (!labelVisible_) {
    labelVisible_ = true;
    changedAt_ = now;
    drawLabel();
    Serial.printf("phase-c1: color=%s view=label\n", colorName());
    return;
  }

  advance(now);
}

const char* Core2DisplayDiagnostic::colorName() const {
  return kColors[colorIndex_].name;
}

bool Core2DisplayDiagnostic::isLabelVisible() const { return labelVisible_; }

void Core2DisplayDiagnostic::drawPlain() {
  M5.Display.fillScreen(kColors[colorIndex_].value);
}

void Core2DisplayDiagnostic::drawLabel() {
  const auto& color = kColors[colorIndex_];
  M5.Display.fillScreen(color.value);
  M5.Display.setTextColor(color.text, color.value);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.drawString(color.name, M5.Display.width() / 2, 62);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString("STATELAMP CORE2 C1", M5.Display.width() / 2, 116);
  M5.Display.drawString("320 x 240  ROTATION 1", M5.Display.width() / 2, 148);
  M5.Display.drawString("FULL-SCREEN LCD TEST", M5.Display.width() / 2, 180);
}

void Core2DisplayDiagnostic::advance(uint32_t now) {
  colorIndex_ = (colorIndex_ + 1) % kColorCount;
  labelVisible_ = false;
  changedAt_ = now;
  drawPlain();
  Serial.printf("phase-c1: color=%s view=plain\n", colorName());
}
