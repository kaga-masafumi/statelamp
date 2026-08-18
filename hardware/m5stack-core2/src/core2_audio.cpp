#include "core2_audio.h"

#include <M5Unified.h>

#if !defined(CORE2_PHASE_C4) && !defined(CORE2_PHASE_C5) && \
    !defined(CORE2_WIFI)
#error "core2_audio.cpp is only for the Core2 Phase C4/C5 targets"
#endif

namespace core2_audio {
namespace {
constexpr uint8_t kVolume = 48;
constexpr uint8_t kChannel = 0;

struct Note {
  uint16_t frequencyHz;
  uint16_t durationMs;
  uint16_t gapMs;
};

constexpr Note kHumanRequired[] = {
    {784, 110, 70}, {988, 110, 70}, {1175, 150, 0}};
constexpr Note kCompleted[] = {{660, 100, 55}, {880, 150, 0}};
constexpr Note kError[] = {{440, 150, 70}, {330, 220, 0}};

const Note* pattern = nullptr;
size_t patternLength = 0;
size_t noteIndex = 0;
uint32_t nextNoteAt = 0;
bool active = false;
bool muted = false;

void stopPattern() {
  M5.Speaker.stop(kChannel);
  pattern = nullptr;
  patternLength = 0;
  noteIndex = 0;
  active = false;
}

void startPattern(const Note* notes, size_t length, uint32_t now) {
  stopPattern();
  pattern = notes;
  patternLength = length;
  noteIndex = 0;
  nextNoteAt = now;
  active = true;
}
}  // namespace

void begin() {
  M5.Speaker.setVolume(kVolume);
  stopPattern();
  Serial.printf("core2-c4: audio ready, volume=%u, startup=silent\n", kVolume);
}

void onStateTransition(StateLampState state, uint32_t now) {
  if (muted) {
    stopPattern();
    Serial.println("core2-audio: notification suppressed, muted=yes");
    return;
  }
  const char* stateName = "silent";
  switch (state) {
    case StateLampState::HumanRequired:
      startPattern(kHumanRequired,
                   sizeof(kHumanRequired) / sizeof(kHumanRequired[0]), now);
      stateName = "human_required";
      break;
    case StateLampState::Completed:
      startPattern(kCompleted, sizeof(kCompleted) / sizeof(kCompleted[0]), now);
      stateName = "completed";
      break;
    case StateLampState::Error:
      startPattern(kError, sizeof(kError) / sizeof(kError[0]), now);
      stateName = "error";
      break;
    default:
      stopPattern();
      return;
  }
  Serial.printf("core2-c4: audio queued, state=%s\n", stateName);
}

void tick(uint32_t now) {
  if (!active || static_cast<int32_t>(now - nextNoteAt) < 0) return;
  if (noteIndex >= patternLength) {
    stopPattern();
    Serial.println("core2-c4: audio complete");
    return;
  }

  const Note& note = pattern[noteIndex++];
  M5.Speaker.tone(note.frequencyHz, note.durationMs, kChannel, false);
  nextNoteAt = now + note.durationMs + note.gapMs;
}

bool isActive() { return active; }

void setMuted(bool nextMuted) {
  if (muted == nextMuted) return;
  muted = nextMuted;
  if (muted) stopPattern();
  Serial.printf("core2-audio: muted=%s\n", muted ? "yes" : "no");
}

bool isMuted() { return muted; }

}  // namespace core2_audio
