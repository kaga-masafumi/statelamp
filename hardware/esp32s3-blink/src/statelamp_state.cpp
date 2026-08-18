#include "statelamp_state.h"

namespace {
constexpr uint32_t COMPLETED_ON_MS = 3000;
}

StateLampController::StateLampController(uint8_t pin, uint8_t brightness)
    : led_(1, pin, NEO_GRB + NEO_KHZ800) {
  led_.setBrightness(brightness);
}

void StateLampController::begin() {
  led_.begin();
  led_.setPixelColor(0, 0);
  led_.show();
  model_.begin();
}

const char *StateLampController::stateName() const { return model_.stateName(); }

bool StateLampController::parseState(const char *value, StateLampState &parsed) const {
  return model_.parseState(value, parsed);
}

bool StateLampController::applyJson(const char *payload, size_t length, String &response) {
  return model_.applyJson(payload, length, response);
}

void StateLampController::setState(StateLampState next) { model_.setState(next); }

uint32_t StateLampController::stateColor(StateLampState value) {
  switch (value) {
    case StateLampState::Working: return led_.Color(0, 150, 255);
    case StateLampState::WaitingApproval: return led_.Color(255, 105, 0);
    case StateLampState::HumanRequired: return led_.Color(255, 70, 0);
    case StateLampState::Completed: return led_.Color(0, 255, 80);
    case StateLampState::Error: return led_.Color(255, 0, 20);
    case StateLampState::Offline: return led_.Color(120, 35, 255);
    case StateLampState::Idle: return 0;
  }
  return 0;
}

void StateLampController::setLed(bool on) {
  led_.setPixelColor(0, on ? stateColor(model_.state()) : 0);
  led_.show();
}

void StateLampController::update(uint32_t now) {
  const uint32_t elapsed = now - model_.stateChangedAt();
  bool on = false;
  switch (model_.state()) {
    case StateLampState::Idle: break;
    case StateLampState::Working: on = (elapsed % 1600) < 800; break;
    case StateLampState::WaitingApproval: on = (elapsed % 300) < 150; break;
    case StateLampState::HumanRequired: {
      const uint32_t phase = elapsed % 1100;
      on = phase < 180 || (phase >= 280 && phase < 460);
      break;
    }
    case StateLampState::Completed: on = elapsed < COMPLETED_ON_MS; break;
    case StateLampState::Error: {
      const uint32_t phase = elapsed % 1600;
      on = phase < 120 || (phase >= 240 && phase < 360);
      break;
    }
    case StateLampState::Offline: on = (elapsed % 3000) < 100; break;
  }
  setLed(on);
}
