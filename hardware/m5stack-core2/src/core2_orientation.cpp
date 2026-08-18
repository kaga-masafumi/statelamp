#include "core2_orientation.h"

#include <M5Unified.h>

#include <cmath>

#if !defined(CORE2_WIFI)
#error "core2_orientation.cpp is only for the Core2 Wi-Fi target"
#endif

namespace core2_orientation {
namespace {
constexpr uint32_t kSampleIntervalMs = 40;
constexpr uint32_t kDwellMs = 400;
constexpr uint32_t kCooldownMs = 800;
constexpr float kGravityMinG = 0.80f;
constexpr float kGravityMaxG = 1.20f;
constexpr float kEnterAxisG = 0.75f;
constexpr float kStayAxisG = 0.60f;
constexpr float kDisplayNormalMaxG = 0.45f;
constexpr float kFilterAlpha = 0.85f;

struct Vector3 { float x; float y; float z; };
Vector3 gravity = {0.0f, 0.0f, 0.0f};
bool filterInitialized = false;
bool imuRead = false;
bool locked = false;
Orientation current = Orientation::LandscapeUpright;
Orientation candidate = Orientation::Unknown;
uint32_t candidateSince = 0;
uint32_t cooldownUntil = 0;
uint32_t lastSampleAt = 0;
bool committedThisTick = false;

float magnitude(const Vector3& value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Orientation classify() {
  if (!filterInitialized || !imuRead) return Orientation::Unknown;
  const float m = magnitude(gravity);
  if (m < kGravityMinG || m > kGravityMaxG ||
      std::fabs(gravity.z) >= kDisplayNormalMaxG) return Orientation::Unknown;
  const float threshold = candidate == current ? kStayAxisG : kEnterAxisG;
  if (gravity.y <= -threshold) return Orientation::LandscapeUpright;
  if (gravity.y >= threshold) return Orientation::LandscapeUpsideDown;
  return Orientation::Unknown;
}
}

const char* name(Orientation orientation) {
  switch (orientation) {
    case Orientation::LandscapeUpright: return "landscape_upright(y-)";
    case Orientation::LandscapeUpsideDown: return "landscape_upside_down(y+)";
    case Orientation::Unknown: return "unknown";
  }
  return "unknown";
}

void begin(uint32_t now) {
  current = Orientation::LandscapeUpright;
  candidate = Orientation::Unknown;
  candidateSince = 0;
  cooldownUntil = 0;
  lastSampleAt = now - kSampleIntervalMs;
  filterInitialized = false;
  imuRead = false;
  locked = false;
  Serial.printf("core2-orientation: enabled=%s upright_rotation=3 "
                "dwell_ms=%u cooldown_ms=%u\n",
                M5.Imu.isEnabled() ? "yes" : "no", kDwellMs, kCooldownMs);
}

bool tick(uint32_t now) {
  committedThisTick = false;
  if (now - lastSampleAt < kSampleIntervalMs) return false;
  lastSampleAt = now;
  M5.Imu.update();
  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  imuRead = M5.Imu.getAccel(&ax, &ay, &az);
  if (!imuRead) return false;
  const Vector3 raw = {ax, ay, az};
  const float rawMagnitude = magnitude(raw);
  if (!filterInitialized) {
    gravity = raw;
    filterInitialized = true;
  } else if (rawMagnitude >= kGravityMinG && rawMagnitude <= kGravityMaxG) {
    gravity.x = kFilterAlpha * gravity.x + (1.0f - kFilterAlpha) * raw.x;
    gravity.y = kFilterAlpha * gravity.y + (1.0f - kFilterAlpha) * raw.y;
    gravity.z = kFilterAlpha * gravity.z + (1.0f - kFilterAlpha) * raw.z;
  }

  const Orientation observed = classify();
  if (locked || now < cooldownUntil || observed == Orientation::Unknown ||
      observed == current) {
    candidate = Orientation::Unknown;
    candidateSince = 0;
    return false;
  }
  if (candidate != observed) {
    candidate = observed;
    candidateSince = now;
    Serial.printf("core2-orientation: candidate=%s dwell_started_ms=%u\n",
                  name(candidate), now);
    return false;
  }
  if (now - candidateSince < kDwellMs) return false;
  current = candidate;
  candidate = Orientation::Unknown;
  candidateSince = 0;
  cooldownUntil = now + kCooldownMs;
  committedThisTick = true;
  Serial.printf("core2-orientation: commit=%s rotation=%u\n", name(current),
                committedRotation());
  return true;
}

bool isLocked() { return locked; }

void setLocked(bool value, uint32_t now) {
  locked = value;
  candidate = Orientation::Unknown;
  candidateSince = 0;
  cooldownUntil = now + kCooldownMs;
  Serial.printf("core2-orientation: mode=%s held=%s rotation=%u\n",
                locked ? "locked" : "auto", name(current),
                committedRotation());
}

Orientation committed() { return current; }

uint8_t committedRotation() {
  return current == Orientation::LandscapeUpright ? 3 : 1;
}
}  // namespace core2_orientation
