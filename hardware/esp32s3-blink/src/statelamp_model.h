#pragma once

#include <Arduino.h>

enum class StateLampState {
  Idle,
  Working,
  WaitingApproval,
  HumanRequired,
  Completed,
  Error,
  Offline,
};

class StateLampStateMachine {
 public:
  void begin();
  bool applyJson(const char* payload, size_t length, String& response);
  bool parseState(const char* value, StateLampState& parsed) const;
  void setState(StateLampState next);
  StateLampState state() const;
  const char* stateName() const;
  uint32_t stateChangedAt() const;

 private:
  const char* stateName(StateLampState value) const;

  StateLampState state_ = StateLampState::Offline;
  uint32_t stateChangedAt_ = 0;
};
