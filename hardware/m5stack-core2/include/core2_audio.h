#pragma once

#include <Arduino.h>

#include "statelamp_model.h"

namespace core2_audio {

void begin();
void onStateTransition(StateLampState state, uint32_t now);
void tick(uint32_t now);
bool isActive();
void setMuted(bool muted);
bool isMuted();

}  // namespace core2_audio
