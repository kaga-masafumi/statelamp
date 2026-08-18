#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include "statelamp_model.h"

class StateLampController {
 public:
  StateLampController(uint8_t pin, uint8_t brightness);

  void begin();
  bool applyJson(const char *payload, size_t length, String &response);
  bool parseState(const char *value, StateLampState &parsed) const;
  void setState(StateLampState next);
  void update(uint32_t now);
  const char *stateName() const;

 private:
  uint32_t stateColor(StateLampState value);
  void setLed(bool on);

  Adafruit_NeoPixel led_;
  StateLampStateMachine model_;
};
