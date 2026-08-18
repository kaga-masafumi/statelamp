#include <Arduino.h>
#include <M5Unified.h>

#include <cmath>

#if !defined(CORE2_ORIENTATION_R1_DIAGNOSTIC)
#error "core2_orientation_r1_diagnostic.cpp is only for the Core2 R1 diagnostic target"
#endif

namespace {
constexpr uint32_t kSampleIntervalMs = 40;
constexpr uint32_t kLogIntervalMs = 200;
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr uint32_t kDwellMs = 400;
constexpr uint32_t kCooldownMs = 800;
constexpr uint32_t kLockHoldMs = 800;
constexpr uint32_t kOverlayMs = 1500;
constexpr float kGravityMinG = 0.80f;
constexpr float kGravityMaxG = 1.20f;
constexpr float kEnterAxisG = 0.75f;
constexpr float kStayAxisG = 0.60f;
constexpr float kDisplayNormalMaxG = 0.45f;
constexpr float kFilterAlpha = 0.85f;

struct Vector3 {
  float x;
  float y;
  float z;
};

enum class Orientation : uint8_t { Unknown, LandscapeA, LandscapeB };

Vector3 raw = {0.0f, 0.0f, 0.0f};
Vector3 gravity = {0.0f, 0.0f, 0.0f};
bool filterInitialized = false;
bool imuRead = false;
bool rotationLocked = false;
Orientation committed = Orientation::LandscapeA;
Orientation candidate = Orientation::Unknown;
uint32_t candidateSince = 0;
uint32_t cooldownUntil = 0;
uint32_t overlayUntil = 0;
const char* overlayText = nullptr;
uint32_t lastSampleAt = 0;
uint32_t lastLogAt = 0;
uint32_t lastHeartbeatAt = 0;
uint32_t rotationCount = 0;

float magnitude(const Vector3& value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

const char* orientationName(Orientation value) {
  switch (value) {
    case Orientation::LandscapeA: return "landscape_a(y-)";
    case Orientation::LandscapeB: return "landscape_b(y+)";
    case Orientation::Unknown: return "unknown";
  }
  return "unknown";
}

// R0 established the sensor signs, and R1 visual inspection established that
// the board's physical upright landscape is M5GFX rotation 3. Keep the
// physical mapping explicit: y- is upright, y+ is upside-down.
const char* rotationName() { return committed == Orientation::LandscapeA ? "3" : "1"; }

Orientation classify() {
  if (!filterInitialized || !imuRead) return Orientation::Unknown;
  const float m = magnitude(gravity);
  if (m < kGravityMinG || m > kGravityMaxG) return Orientation::Unknown;
  if (std::fabs(gravity.z) >= kDisplayNormalMaxG) return Orientation::Unknown;

  const float threshold = candidate == committed ? kStayAxisG : kEnterAxisG;
  if (gravity.y <= -threshold) return Orientation::LandscapeA;
  if (gravity.y >= threshold) return Orientation::LandscapeB;
  return Orientation::Unknown;
}

void drawScreen() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  M5.Display.fillScreen(0x101820U);
  M5.Display.setTextColor(0xFFFFFFU, 0x101820U);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  M5.Display.drawString("CORE2 ORIENTATION R1", 12, 12);
  M5.Display.setTextSize(1);
  M5.Display.drawString("Visual diagnostic: rotation 1 <-> 3", 12, 40);
  M5.Display.printf("mode: %s   committed: %s   rotation: %s",
                   rotationLocked ? "LOCKED" : "AUTO", orientationName(committed),
                   rotationName());
  M5.Display.setCursor(12, 76);
  M5.Display.printf("candidate: %s", orientationName(candidate));
  M5.Display.setCursor(12, 94);
  M5.Display.printf("gravity: %+.2f %+.2f %+.2f", gravity.x, gravity.y,
                    gravity.z);
  M5.Display.setCursor(12, 112);
  M5.Display.printf("pose: %s  |g|=%.2f", orientationName(classify()),
                    magnitude(gravity));
  M5.Display.setCursor(12, height - 54);
  M5.Display.printf("A hold: lock   B/C: inert   flips: %u", rotationCount);
  if (overlayText != nullptr && millis() < overlayUntil) {
    M5.Display.fillRoundRect(width / 2 - 100, height / 2 - 22, 200, 44, 8,
                             0xFFFFFFU);
    M5.Display.setTextColor(0x101820U, 0xFFFFFFU);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString(overlayText, width / 2, height / 2);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(0xFFFFFFU, 0x101820U);
  }
}

void showOverlay(const char* text, uint32_t now) {
  overlayText = text;
  overlayUntil = now + kOverlayMs;
  drawScreen();
}

void setCommitted(Orientation next, uint32_t now) {
  if (next == committed || next == Orientation::Unknown) return;
  committed = next;
  const uint8_t rotation = committed == Orientation::LandscapeA ? 3 : 1;
  M5.Display.setRotation(rotation);
  ++rotationCount;
  cooldownUntil = now + kCooldownMs;
  candidate = Orientation::Unknown;
  candidateSince = 0;
  Serial.printf("core2-r1: commit orientation=%s rotation=%u dwell_ms=%u "
                "cooldown_ms=%u\n",
                orientationName(committed), rotation, kDwellMs, kCooldownMs);
  drawScreen();
}

void updateCandidate(uint32_t now) {
  const Orientation observed = classify();
  if (rotationLocked || now < cooldownUntil || observed == Orientation::Unknown ||
      observed == committed) {
    candidate = Orientation::Unknown;
    candidateSince = 0;
    return;
  }
  if (candidate != observed) {
    candidate = observed;
    candidateSince = now;
    Serial.printf("core2-r1: candidate=%s dwell_started_ms=%u\n",
                  orientationName(candidate), now);
    return;
  }
  if (now - candidateSince >= kDwellMs) setCommitted(candidate, now);
}

void sample(uint32_t now) {
  if (now - lastSampleAt < kSampleIntervalMs) return;
  lastSampleAt = now;
  M5.Imu.update();
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  imuRead = M5.Imu.getAccel(&ax, &ay, &az);
  if (!imuRead) return;
  raw = {ax, ay, az};
  const float m = magnitude(raw);
  if (!filterInitialized) {
    gravity = raw;
    filterInitialized = true;
  } else if (m >= kGravityMinG && m <= kGravityMaxG) {
    gravity.x = kFilterAlpha * gravity.x + (1.0f - kFilterAlpha) * raw.x;
    gravity.y = kFilterAlpha * gravity.y + (1.0f - kFilterAlpha) * raw.y;
    gravity.z = kFilterAlpha * gravity.z + (1.0f - kFilterAlpha) * raw.z;
  }
}

void logButtons(uint32_t now) {
  if (M5.BtnA.wasHold()) {
    rotationLocked = !rotationLocked;
    candidate = Orientation::Unknown;
    candidateSince = 0;
    Serial.printf("core2-r1: button=A hold mode=%s held_rotation=%s\n",
                  rotationLocked ? "locked" : "auto", rotationName());
    showOverlay(rotationLocked ? "Rotation locked" : "Auto rotation", now);
  }
  if (M5.BtnB.wasHold()) {
    Serial.printf("core2-r1: button=B hold action=inert mode=%s\n",
                  rotationLocked ? "locked" : "auto");
  }
  if (M5.BtnC.wasHold()) {
    Serial.printf("core2-r1: button=C hold action=inert mode=%s\n",
                  rotationLocked ? "locked" : "auto");
  }
}

void logSample(uint32_t now) {
  if (now - lastLogAt < kLogIntervalMs) return;
  lastLogAt = now;
  Serial.printf("core2-r1: sample uptime_ms=%u raw=(%+.3f,%+.3f,%+.3f) "
                "gravity=(%+.3f,%+.3f,%+.3f) magnitude=%.3f observed=%s "
                "candidate=%s committed=%s rotation=%s mode=%s\n",
                now, raw.x, raw.y, raw.z, gravity.x, gravity.y, gravity.z,
                magnitude(gravity), orientationName(classify()),
                orientationName(candidate), orientationName(committed),
                rotationName(), rotationLocked ? "locked" : "auto");
}
}  // namespace

void setup() {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.clear_display = false;
  config.output_power = false;
  config.internal_imu = true;
  config.internal_rtc = false;
  config.internal_spk = false;
  config.internal_mic = false;
  M5.begin(config);
  M5.BtnA.setHoldThresh(kLockHoldMs);
  M5.BtnB.setHoldThresh(kLockHoldMs);
  M5.BtnC.setHoldThresh(kLockHoldMs);
  M5.Display.setRotation(3);
  drawScreen();
  Serial.println();
  Serial.println("StateLamp Core2 orientation diagnostic R1");
  Serial.println("scope: visual rotation 1/3, A lock, B/C inert");
  Serial.println("production UI, Wi-Fi, Bridge, and audio: absent");
  Serial.printf("core2-r1: imu_enabled=%s rotation=%s dwell_ms=%u "
                "cooldown_ms=%u\n",
                M5.Imu.isEnabled() ? "yes" : "no", rotationName(), kDwellMs,
                kCooldownMs);
  lastSampleAt = millis() - kSampleIntervalMs;
  lastHeartbeatAt = millis();
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  sample(now);
  updateCandidate(now);
  logButtons(now);
  if (overlayText != nullptr && now >= overlayUntil) {
    overlayText = nullptr;
    drawScreen();
  }
  logSample(now);
  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf("core2-r1: alive imu=%s mode=%s rotation=%s flips=%u "
                  "free_heap=%u uptime_ms=%u\n",
                  M5.Imu.isEnabled() ? "yes" : "no",
                  rotationLocked ? "locked" : "auto", rotationName(),
                  rotationCount, ESP.getFreeHeap(), now);
  }
  delay(5);
}
