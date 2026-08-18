#pragma once

#include <Arduino.h>

namespace core2_orientation {
enum class Orientation : uint8_t { Unknown, LandscapeUpright, LandscapeUpsideDown };

void begin(uint32_t now);
bool tick(uint32_t now);
bool isLocked();
void setLocked(bool locked, uint32_t now);
Orientation committed();
uint8_t committedRotation();
const char* name(Orientation orientation);
}
