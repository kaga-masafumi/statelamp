#pragma once

#include <M5Unified.h>

class Core2DisplayDiagnostic {
 public:
  static constexpr uint8_t kRotation = 1;

  void begin();
  void update(uint32_t now);
  const char* colorName() const;
  bool isLabelVisible() const;

 private:
  static constexpr uint32_t kPlainDurationMs = 2000;
  static constexpr uint32_t kLabelDurationMs = 3000;

  void drawPlain();
  void drawLabel();
  void advance(uint32_t now);

  uint8_t colorIndex_ = 0;
  bool labelVisible_ = false;
  uint32_t changedAt_ = 0;
};
