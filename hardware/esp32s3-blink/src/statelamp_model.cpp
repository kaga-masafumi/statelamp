#include "statelamp_model.h"

#include <ArduinoJson.h>

void StateLampStateMachine::begin() { stateChangedAt_ = millis(); }

const char* StateLampStateMachine::stateName(StateLampState value) const {
  switch (value) {
    case StateLampState::Idle: return "idle";
    case StateLampState::Working: return "working";
    case StateLampState::WaitingApproval: return "waiting_approval";
    case StateLampState::HumanRequired: return "human_required";
    case StateLampState::Completed: return "completed";
    case StateLampState::Error: return "error";
    case StateLampState::Offline: return "offline";
  }
  return "unknown";
}

const char* StateLampStateMachine::stateName() const { return stateName(state_); }

StateLampState StateLampStateMachine::state() const { return state_; }

uint32_t StateLampStateMachine::stateChangedAt() const { return stateChangedAt_; }

bool StateLampStateMachine::parseState(const char* value, StateLampState& parsed) const {
  if (strcmp(value, "idle") == 0) parsed = StateLampState::Idle;
  else if (strcmp(value, "working") == 0) parsed = StateLampState::Working;
  else if (strcmp(value, "waiting_approval") == 0) parsed = StateLampState::WaitingApproval;
  else if (strcmp(value, "human_required") == 0) parsed = StateLampState::HumanRequired;
  else if (strcmp(value, "completed") == 0) parsed = StateLampState::Completed;
  else if (strcmp(value, "error") == 0) parsed = StateLampState::Error;
  else return false;
  return true;
}

bool StateLampStateMachine::applyJson(const char* payload, size_t length,
                                   String& response) {
  if (length == 0 || length > 512) {
    response = "{\"ok\":false,\"error\":\"invalid_length\"}";
    return false;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, payload, length);
  if (error || !document["state"].is<const char*>()) {
    response = "{\"ok\":false,\"error\":\"invalid_json\"}";
    return false;
  }

  StateLampState parsed;
  if (!parseState(document["state"], parsed)) {
    response = "{\"ok\":false,\"error\":\"unknown_state\"}";
    return false;
  }

  setState(parsed);
  response = String("{\"ok\":true,\"state\":\"") + stateName() + "\"}";
  return true;
}

void StateLampStateMachine::setState(StateLampState next) {
  if (next == state_) return;
  state_ = next;
  stateChangedAt_ = millis();
  Serial.printf("State: %s\n", stateName());
}
