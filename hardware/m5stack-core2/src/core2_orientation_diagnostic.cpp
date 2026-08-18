#include <Arduino.h>
#include <M5Unified.h>

#include <cmath>

#if !defined(CORE2_ORIENTATION_DIAGNOSTIC)
#error "core2_orientation_diagnostic.cpp is only for the Core2 orientation diagnostic target"
#endif

namespace {
constexpr uint32_t kSampleIntervalMs = 40;
constexpr uint32_t kLogIntervalMs = 200;
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr uint32_t kButtonHoldMs = 800;
constexpr float kGravityMinG = 0.80f;
constexpr float kGravityMaxG = 1.20f;
constexpr float kVerticalAxisEnterG = 0.75f;
constexpr float kDisplayNormalMaxG = 0.45f;
constexpr float kFilterAlpha = 0.85f;

struct Vector3 {
  float x;
  float y;
  float z;
};

Vector3 raw = {0.0f, 0.0f, 0.0f};
Vector3 gravity = {0.0f, 0.0f, 0.0f};
bool filterInitialized = false;
bool imuRead = false;
bool rotationLocked = false;
uint32_t lastSampleAt = 0;
uint32_t lastLogAt = 0;
uint32_t lastHeartbeatAt = 0;

float magnitude(const Vector3& value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

const char* axisName(float x, float y, float z) {
  const float ax = std::fabs(x);
  const float ay = std::fabs(y);
  const float az = std::fabs(z);
  if (ax < kVerticalAxisEnterG && ay < kVerticalAxisEnterG &&
      az < kVerticalAxisEnterG) {
    return "unknown";
  }
  if (ax >= ay && ax >= az) return x >= 0.0f ? "x+" : "x-";
  if (ay >= ax && ay >= az) return y >= 0.0f ? "y+" : "y-";
  return z >= 0.0f ? "z+" : "z-";
}

const char* poseName() {
  if (!filterInitialized || !imuRead) return "no_data";
  const float filteredMagnitude = magnitude(gravity);
  if (filteredMagnitude < kGravityMinG || filteredMagnitude > kGravityMaxG) {
    return "motion_or_bad_magnitude";
  }

  // For Core2's screen, z is the display-normal candidate. This is a
  // diagnostic hypothesis only; R0 evidence must confirm the axis mapping.
  if (std::fabs(gravity.z) >= kDisplayNormalMaxG) return "face_up_or_down";
  if (std::fabs(gravity.x) >= kVerticalAxisEnterG ||
      std::fabs(gravity.y) >= kVerticalAxisEnterG) {
    return axisName(gravity.x, gravity.y, gravity.z);
  }
  return "unknown";
}

void drawScreen() {
  M5.Display.setRotation(1);
  M5.Display.fillScreen(0x101820U);
  M5.Display.setTextColor(0xFFFFFFU, 0x101820U);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(12, 12);
  M5.Display.println("CORE2 ORIENTATION R0");
  M5.Display.setTextSize(1);
  M5.Display.setCursor(12, 42);
  M5.Display.println("Read-only: no display rotation");
  M5.Display.setCursor(12, 66);
  M5.Display.printf("IMU: %s  mode: %s", imuRead ? "ready" : "error",
                   rotationLocked ? "LOCK" : "AUTO");
  M5.Display.setCursor(12, 86);
  M5.Display.printf("raw  %+.2f %+.2f %+.2f", raw.x, raw.y, raw.z);
  M5.Display.setCursor(12, 104);
  M5.Display.printf("grav %+.2f %+.2f %+.2f", gravity.x, gravity.y,
                   gravity.z);
  M5.Display.setCursor(12, 122);
  M5.Display.printf("|g| %.2f  axis %s", magnitude(gravity),
                   axisName(gravity.x, gravity.y, gravity.z));
  M5.Display.setCursor(12, 140);
  M5.Display.printf("pose %s", poseName());
  M5.Display.setCursor(12, 178);
  M5.Display.println("A hold: observe lock toggle");
  M5.Display.setCursor(12, 194);
  M5.Display.println("B/C hold: observe independently");
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
  const float rawMagnitude = magnitude(raw);
  if (!filterInitialized) {
    gravity = raw;
    filterInitialized = true;
  } else if (rawMagnitude >= kGravityMinG && rawMagnitude <= kGravityMaxG) {
    gravity.x = kFilterAlpha * gravity.x + (1.0f - kFilterAlpha) * raw.x;
    gravity.y = kFilterAlpha * gravity.y + (1.0f - kFilterAlpha) * raw.y;
    gravity.z = kFilterAlpha * gravity.z + (1.0f - kFilterAlpha) * raw.z;
  }
}

void logButtons(uint32_t now) {
  if (M5.BtnA.wasHold()) {
    rotationLocked = !rotationLocked;
    Serial.printf("core2-r0: button=A event=hold mode=%s uptime_ms=%u\n",
                  rotationLocked ? "locked" : "auto", now);
    drawScreen();
  }
  if (M5.BtnB.wasHold()) {
    Serial.printf("core2-r0: button=B event=hold action=observation_only "
                  "mode=%s uptime_ms=%u\n",
                  rotationLocked ? "locked" : "auto", now);
  }
  if (M5.BtnC.wasHold()) {
    Serial.printf("core2-r0: button=C event=hold action=observation_only "
                  "mode=%s uptime_ms=%u\n",
                  rotationLocked ? "locked" : "auto", now);
  }
}

void logSample(uint32_t now) {
  if (now - lastLogAt < kLogIntervalMs) return;
  lastLogAt = now;
  Serial.printf(
      "core2-r0: sample uptime_ms=%u raw=(%+.3f,%+.3f,%+.3f) "
      "gravity=(%+.3f,%+.3f,%+.3f) magnitude=%.3f pose=%s axis=%s mode=%s\n",
      now, raw.x, raw.y, raw.z, gravity.x, gravity.y, gravity.z,
      magnitude(gravity), poseName(), axisName(gravity.x, gravity.y, gravity.z),
      rotationLocked ? "locked" : "auto");
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

  M5.BtnA.setHoldThresh(kButtonHoldMs);
  M5.BtnB.setHoldThresh(kButtonHoldMs);
  M5.BtnC.setHoldThresh(kButtonHoldMs);
  drawScreen();

  Serial.println();
  Serial.println("StateLamp Core2 orientation diagnostic R0");
  Serial.println("scope: IMU/filter/pose and A-B-C hold observation only");
  Serial.println("setRotation: never called by this target");
  Serial.printf("core2-r0: imu_enabled=%s display=%dx%d sample_ms=%u "
                "alpha=%.2f hold_ms=%u\n",
                M5.Imu.isEnabled() ? "yes" : "no", M5.Display.width(),
                M5.Display.height(), kSampleIntervalMs, kFilterAlpha,
                kButtonHoldMs);
  lastSampleAt = millis() - kSampleIntervalMs;
  lastHeartbeatAt = millis();
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  sample(now);
  logButtons(now);
  logSample(now);
  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf("core2-r0: alive imu=%s mode=%s free_heap=%u uptime_ms=%u\n",
                  M5.Imu.isEnabled() ? "yes" : "no",
                  rotationLocked ? "locked" : "auto", ESP.getFreeHeap(), now);
  }
  delay(5);
}
