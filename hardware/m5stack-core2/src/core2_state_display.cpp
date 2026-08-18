#include "core2_state_display.h"

#include <M5Unified.h>

#if !defined(CORE2_PHASE_C2)
#error "core2_state_display.cpp is only for the Core2 Phase C2 target"
#endif

namespace core2_state_display {
namespace {
uint32_t stateColor(StateLampState state) {
  switch (state) {
    case StateLampState::Idle: return 0x000000U;
    case StateLampState::Working: return 0x009CFFU;
    case StateLampState::WaitingApproval: return 0xFFA500U;
    case StateLampState::HumanRequired: return 0xFF4500U;
    case StateLampState::Completed: return 0x00FF52U;
    case StateLampState::Error: return 0xFF0010U;
    case StateLampState::Offline: return 0x7800FFU;
  }
  return 0x000000U;
}
}  // namespace

void begin() { M5.Display.setRotation(1); }

void render(StateLampState state) { M5.Display.fillScreen(stateColor(state)); }

}  // namespace core2_state_display
