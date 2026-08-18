#pragma once

#include <Arduino.h>

#include "statelamp_model.h"

namespace core2_ui {
struct AgentView {
  String agent;
  StateLampState state;
};

void begin();
void render(StateLampState state, const String& message = String());
void renderOverview(const AgentView* agents, size_t count, size_t primaryIndex);
void tick(uint32_t now);
void setAudioMuted(bool muted);
void setDisplayRotation(uint8_t rotation);
void showPresentationOverlay(const char* text, uint32_t now);
bool isRotationLocked();
void setRotationLocked(bool locked);
}
