#include "core2_ui.h"

#include <M5Unified.h>

#if !defined(CORE2_PHASE_C3) && !defined(CORE2_PHASE_C4) && \
    !defined(CORE2_PHASE_C5) && !defined(CORE2_WIFI)
#error "core2_ui.cpp is only for the Core2 Phase C3/C4/C5 targets"
#endif

namespace core2_ui {
namespace {
constexpr uint32_t kWhite = 0xFFFFFFU;
constexpr uint32_t kNearBlack = 0x101010U;
constexpr uint32_t kCatWhite = 0xFFFDF5U;
constexpr uint32_t kCatPink = 0xFF8088U;
constexpr uint32_t kMutedLight = 0xD8E0E8U;
constexpr uint32_t kMutedDark = 0x303840U;
constexpr uint32_t kIdleBlinkPeriodMs = 4200;
constexpr uint32_t kIdleBlinkDurationMs = 180;
constexpr uint32_t kWorkingGazePeriodMs = 700;
constexpr size_t kMessageColumns = 24;
constexpr size_t kMessageRows = 2;

StateLampState currentState = StateLampState::Offline;
uint8_t currentAnimationFrame = 0;
bool audioMuted = false;
bool rotationLocked = false;
uint32_t overlayUntil = 0;
const char* overlayText = nullptr;
enum class ViewMode : uint8_t { Single, Overview };
ViewMode viewMode = ViewMode::Single;
String currentMessage;
AgentView currentAgents[8];
size_t currentAgentCount = 0;
size_t currentPrimaryIndex = 0;

struct Appearance {
  uint32_t background;
  uint32_t foreground;
  const char* line1;
  const char* line2;
};

Appearance appearance(StateLampState state) {
  switch (state) {
    case StateLampState::Idle: return {0x000000U, kWhite, "IDLE", nullptr};
    case StateLampState::Working:
      return {0x009CFFU, kWhite, "WORKING", nullptr};
    case StateLampState::WaitingApproval:
      return {0xFFA500U, kNearBlack, "WAITING", "APPROVAL"};
    case StateLampState::HumanRequired:
      return {0xFF4500U, kNearBlack, "HUMAN", "REQUIRED"};
    case StateLampState::Completed:
      return {0x00FF52U, kNearBlack, "COMPLETED", nullptr};
    case StateLampState::Error: return {0xFF0010U, kWhite, "ERROR", nullptr};
    case StateLampState::Offline:
      return {0x7800FFU, kWhite, "OFFLINE", nullptr};
  }
  return {0x000000U, kWhite, "UNKNOWN", nullptr};
}

void rect(int x, int y, int width, int height, uint32_t color) {
  M5.Display.fillRect(x, y, width, height, color);
}

void drawCatBase() {
  // Tall, stepped ears keep a sharp silhouette in the low-resolution UI.
  rect(28, 28, 8, 8, kNearBlack);
  rect(24, 36, 16, 10, kNearBlack);
  rect(20, 46, 30, 22, kNearBlack);
  rect(116, 28, 8, 8, kNearBlack);
  rect(112, 36, 16, 10, kNearBlack);
  rect(102, 46, 30, 22, kNearBlack);
  rect(28, 42, 10, 18, kCatPink);
  rect(114, 42, 10, 18, kCatPink);

  // White face with black hachiware patches and a central white blaze.
  rect(24, 52, 104, 54, kNearBlack);
  rect(30, 56, 92, 44, kCatWhite);
  rect(30, 56, 34, 12, kNearBlack);
  rect(36, 68, 22, 6, kNearBlack);
  rect(88, 56, 34, 12, kNearBlack);
  rect(94, 68, 22, 6, kNearBlack);
  rect(64, 52, 24, 34, kCatWhite);
  rect(58, 62, 36, 16, kCatWhite);

  rect(73, 80, 6, 5, kCatPink);
  rect(41, 88, 12, 3, kCatPink);
  rect(99, 88, 12, 3, kCatPink);
  rect(14, 81, 18, 3, kNearBlack);
  rect(17, 89, 17, 3, kNearBlack);
  rect(120, 81, 18, 3, kNearBlack);
  rect(118, 89, 17, 3, kNearBlack);
}

void drawOpenEyes(int pupilOffset) {
  rect(47, 67, 14, 16, kNearBlack);
  rect(91, 67, 14, 16, kNearBlack);
  rect(51 + pupilOffset, 70, 4, 6, kWhite);
  rect(95 + pupilOffset, 70, 4, 6, kWhite);
}

void drawCatExpression(StateLampState state, uint8_t frame) {
  switch (state) {
    case StateLampState::Idle:
      if (frame == 1) drawOpenEyes(2);
      else {
        rect(45, 75, 18, 4, kNearBlack);
        rect(89, 75, 18, 4, kNearBlack);
      }
      rect(67, 89, 8, 3, kNearBlack);
      rect(77, 89, 8, 3, kNearBlack);
      break;
    case StateLampState::Working:
      drawOpenEyes(frame == 0 ? 0 : 4);
      rect(72, 89, 8, 3, kNearBlack);
      rect(80, 86, 10, 3, kNearBlack);
      break;
    case StateLampState::WaitingApproval:
      drawOpenEyes(2);
      rect(70, 90, 12, 5, kNearBlack);
      M5.Display.setTextColor(kNearBlack);
      M5.Display.setTextDatum(middle_center);
      M5.Display.setTextSize(3);
      M5.Display.drawString("?", 76, 40);
      break;
    case StateLampState::HumanRequired:
      rect(44, 65, 18, 20, kNearBlack);
      rect(90, 65, 18, 20, kNearBlack);
      rect(49, 69, 5, 7, kWhite);
      rect(95, 69, 5, 7, kWhite);
      rect(70, 88, 12, 9, kNearBlack);
      M5.Display.setTextColor(kNearBlack);
      M5.Display.setTextDatum(middle_center);
      M5.Display.setTextSize(3);
      M5.Display.drawString("!", 76, 40);
      break;
    case StateLampState::Completed:
      rect(45, 72, 7, 4, kNearBlack);
      rect(52, 76, 11, 4, kNearBlack);
      rect(89, 76, 11, 4, kNearBlack);
      rect(100, 72, 7, 4, kNearBlack);
      rect(61, 87, 5, 4, kNearBlack);
      rect(65, 91, 6, 4, kNearBlack);
      rect(70, 95, 12, 4, kNearBlack);
      rect(81, 91, 6, 4, kNearBlack);
      rect(86, 87, 5, 4, kNearBlack);
      break;
    case StateLampState::Error:
      for (int offset = 0; offset < 3; ++offset) {
        rect(45 + offset * 5, 67 + offset * 5, 5, 5, kNearBlack);
        rect(55 - offset * 5, 67 + offset * 5, 5, 5, kNearBlack);
        rect(92 + offset * 5, 67 + offset * 5, 5, 5, kNearBlack);
        rect(102 - offset * 5, 67 + offset * 5, 5, 5, kNearBlack);
      }
      rect(67, 92, 18, 4, kNearBlack);
      break;
    case StateLampState::Offline:
      rect(47, 68, 14, 14, kNearBlack);
      rect(91, 68, 14, 14, kNearBlack);
      rect(51, 71, 6, 8, 0x808080U);
      rect(95, 71, 6, 8, 0x808080U);
      rect(70, 91, 12, 3, kNearBlack);
      break;
  }
}

String displaySafeMessage(const String& input) {
  String output;
  output.reserve(kMessageColumns * kMessageRows);
  const size_t limit = kMessageColumns * kMessageRows;
  bool truncated = false;
  for (size_t index = 0; index < input.length();) {
    if (output.length() >= limit) {
      truncated = true;
      break;
    }
    const uint8_t byte = static_cast<uint8_t>(input[index]);
    if (byte >= 0x20 && byte <= 0x7E) {
      output += static_cast<char>(byte);
      ++index;
      continue;
    }
    if (byte == '\n' || byte == '\r' || byte == '\t') {
      output += ' ';
      ++index;
      continue;
    }
    size_t sequenceLength = 1;
    if ((byte & 0xE0U) == 0xC0U) sequenceLength = 2;
    else if ((byte & 0xF0U) == 0xE0U) sequenceLength = 3;
    else if ((byte & 0xF8U) == 0xF0U) sequenceLength = 4;
    bool valid = sequenceLength > 1 && index + sequenceLength <= input.length();
    for (size_t part = 1; valid && part < sequenceLength; ++part) {
      valid = (static_cast<uint8_t>(input[index + part]) & 0xC0U) == 0x80U;
    }
    output += '?';
    index += valid ? sequenceLength : 1;
  }
  if (truncated && output.length() >= 3) {
    output.remove(output.length() - 3);
    output += "...";
  }
  return output;
}

void drawMessage(const String& input, uint32_t foreground, uint32_t background) {
  const String message = displaySafeMessage(input);
  rect(12, 128, 296, 58, background);
  if (message.isEmpty()) return;
  M5.Display.setTextColor(foreground, background);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  for (size_t row = 0; row < kMessageRows; ++row) {
    const size_t start = row * kMessageColumns;
    if (start >= message.length()) break;
    M5.Display.drawString(message.substring(start, start + kMessageColumns),
                          14, 132 + static_cast<int>(row) * 24);
  }
}

void redrawAnimatedCat(uint8_t frame) {
  drawCatBase();
  drawCatExpression(currentState, frame);
}

void drawFooter() {
  const Appearance ui = appearance(currentState);
  const uint32_t background = ui.foreground == kWhite ? kMutedDark : kMutedLight;
  const uint32_t foreground = ui.foreground == kWhite ? kWhite : kNearBlack;
  rect(0, 205, 320, 35, background);
#if defined(CORE2_WIFI)
  M5.Display.setTextColor(foreground, background);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString("STATELAMP", 160, 212);
  M5.Display.drawString("WIFI", 24, 231);
  M5.Display.drawString(rotationLocked ? "LOCK" : "AUTO", 160, 231);
  M5.Display.drawString(audioMuted ? "MUTED" : "AUDIO", 285, 231);
#else
  M5.Display.setTextColor(foreground, background);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("STATELAMP", audioMuted ? 118 : 160, 223);
  if (audioMuted) {
    M5.Display.setTextSize(1);
    M5.Display.drawString("MUTED", 270, 223);
  }
#endif
}

void drawPresentationOverlay(uint32_t now) {
  if (overlayText == nullptr || now >= overlayUntil) return;
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  M5.Display.fillRoundRect(width / 2 - 100, height / 2 - 22, 200, 44, 8,
                           kWhite);
  M5.Display.setTextColor(kNearBlack, kWhite);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString(overlayText, width / 2, height / 2);
  M5.Display.setTextDatum(top_left);
}

String compactLabel(StateLampState state) {
  switch (state) {
    case StateLampState::Idle: return "IDLE";
    case StateLampState::Working: return "WORKING";
    case StateLampState::WaitingApproval: return "APPROVAL";
    case StateLampState::HumanRequired: return "HUMAN";
    case StateLampState::Completed: return "DONE";
    case StateLampState::Error: return "ERROR";
    case StateLampState::Offline: return "OFFLINE";
  }
  return "UNKNOWN";
}

String compactAgent(const String& agent) {
  if (agent.isEmpty()) return "?";
  String initial = agent.substring(0, 1);
  initial.toUpperCase();
  return initial;
}
}  // namespace

void begin() { M5.Display.setRotation(1); }

void render(StateLampState state, const String& message) {
  viewMode = ViewMode::Single;
  currentState = state;
  currentMessage = message;
  currentAnimationFrame = 0;
  const Appearance ui = appearance(state);
  M5.Display.fillScreen(ui.background);
  drawCatBase();
  drawCatExpression(state, currentAnimationFrame);

  M5.Display.setTextColor(ui.foreground, ui.background);
  M5.Display.setTextDatum(middle_center);
  if (ui.line2 == nullptr) {
    M5.Display.setTextSize(strlen(ui.line1) > 8 ? 2 : 3);
    M5.Display.drawString(ui.line1, 230, 72);
  } else {
    M5.Display.setTextSize(2);
    M5.Display.drawString(ui.line1, 230, 58);
    M5.Display.drawString(ui.line2, 230, 84);
  }
  drawMessage(message, ui.foreground, ui.background);

  drawFooter();
}

void renderOverview(const AgentView* agents, size_t count, size_t primaryIndex) {
  constexpr size_t kVisibleAgents = 3;
  viewMode = ViewMode::Overview;
  currentAgentCount = count > 8 ? 8 : count;
  currentPrimaryIndex = primaryIndex;
  for (size_t index = 0; index < currentAgentCount; ++index) {
    currentAgents[index] = agents[index];
  }
  currentAnimationFrame = 0;
  if (count == 0 || primaryIndex >= count) {
    render(StateLampState::Offline, "No agent data");
    return;
  }

  currentState = agents[primaryIndex].state;
  const Appearance primary = appearance(currentState);
  M5.Display.fillScreen(primary.background);

  // Keep the StateLamp's character: the most urgent agent gets the large cat and
  // speech bubble, while compact cards below preserve the overview.
  drawCatBase();
  drawCatExpression(currentState, currentAnimationFrame);
  M5.Display.fillRoundRect(146, 15, 164, 98, 12, kCatWhite);
  M5.Display.setTextColor(kNearBlack, kCatWhite);
  M5.Display.setTextDatum(middle_center);
  const bool needsHuman = currentState == StateLampState::HumanRequired;
  M5.Display.setTextSize(1);
  M5.Display.drawString(needsHuman ? "NEEDS YOU!" : "ON DUTY", 228, 29);
  M5.Display.setTextSize(4);
  M5.Display.drawString(compactAgent(agents[primaryIndex].agent), 228, 58);
  M5.Display.setTextSize(2);
  M5.Display.drawString(compactLabel(currentState), 228, 94);

  const size_t visible = count < kVisibleAgents ? count : kVisibleAgents;
  size_t sourceIndex = primaryIndex;
  for (size_t slot = 0; slot < visible; ++slot) {
    if (slot > 0) {
      sourceIndex = slot - 1;
      if (sourceIndex >= primaryIndex) ++sourceIndex;
    }
    const int x = 7 + static_cast<int>(slot) * 103;
    const int y = 125;
    const Appearance row = appearance(agents[sourceIndex].state);
    const bool selected = sourceIndex == primaryIndex;
    const uint32_t background = row.background;
    const uint32_t foreground = row.foreground;
    M5.Display.fillRoundRect(x, y, 99, 70, 10, background);
    if (selected) M5.Display.drawRoundRect(x, y, 99, 70, 10, kWhite);
    M5.Display.setTextColor(foreground, background);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(3);
    M5.Display.drawString(compactAgent(agents[sourceIndex].agent), x + 49,
                          y + 23);
    M5.Display.setTextSize(2);
    M5.Display.drawString(compactLabel(agents[sourceIndex].state), x + 49,
                          y + 53);
  }
  if (count > kVisibleAgents) {
    M5.Display.setTextColor(kWhite, kNearBlack);
    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextSize(1);
    M5.Display.drawString(String("+") + String(count - kVisibleAgents), 310,
                          201);
  }
  drawFooter();
}

void tick(uint32_t now) {
  uint8_t nextFrame = 0;
  bool animated = true;
  if (currentState == StateLampState::Idle) {
    nextFrame = (now % kIdleBlinkPeriodMs) < kIdleBlinkDurationMs ? 1 : 0;
  } else if (currentState == StateLampState::Working) {
    nextFrame = (now / kWorkingGazePeriodMs) % 2;
  } else {
    animated = false;
  }
  if (animated && nextFrame != currentAnimationFrame) {
    currentAnimationFrame = nextFrame;
    redrawAnimatedCat(currentAnimationFrame);
  }
  if (overlayText != nullptr && now < overlayUntil) {
    drawPresentationOverlay(now);
  }
  if (overlayText != nullptr && now >= overlayUntil) {
    overlayText = nullptr;
    if (viewMode == ViewMode::Overview) {
      renderOverview(currentAgents, currentAgentCount, currentPrimaryIndex);
    } else {
      render(currentState, currentMessage);
    }
  }
}

void setAudioMuted(bool muted) {
  if (audioMuted == muted) return;
  audioMuted = muted;
  drawFooter();
}

void setDisplayRotation(uint8_t rotation) {
  M5.Display.setRotation(rotation);
  if (viewMode == ViewMode::Overview) {
    renderOverview(currentAgents, currentAgentCount, currentPrimaryIndex);
  } else {
    render(currentState, currentMessage);
  }
}

void showPresentationOverlay(const char* text, uint32_t now) {
  overlayText = text;
  overlayUntil = now + 1500;
  drawPresentationOverlay(now);
}

bool isRotationLocked() { return rotationLocked; }

void setRotationLocked(bool locked) {
  if (rotationLocked == locked) return;
  rotationLocked = locked;
  drawFooter();
}
}  // namespace core2_ui
