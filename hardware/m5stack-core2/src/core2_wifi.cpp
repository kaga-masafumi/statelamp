#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "statelamp_model.h"
#include "core2_audio.h"
#include "core2_orientation.h"
#include "core2_touch.h"
#include "core2_ui.h"
#include "device_config.h"

#if !defined(CORE2_WIFI) || !defined(CORE2_PHASE_C5)
#error "core2_wifi.cpp requires CORE2_WIFI and CORE2_PHASE_C5"
#endif

namespace {
constexpr uint32_t kPollIntervalMs = 1000;
constexpr uint32_t kHttpTimeoutMs = 750;
constexpr uint32_t kWifiRetryIntervalMs = 5000;
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr size_t kMaxAgents = 8;

StateLampStateMachine StateLamp;
uint32_t lastPollAt = 0;
uint32_t lastWifiAttemptAt = 0;
uint32_t lastHeartbeatAt = 0;
String lastMessage;
core2_ui::AgentView agents[kMaxAgents];
size_t agentCount = 0;
String agentsUrl;
String lastOverviewSignature;

uint8_t priority(StateLampState state) {
  switch (state) {
    case StateLampState::HumanRequired: return 7;
    case StateLampState::Error: return 6;
    case StateLampState::WaitingApproval: return 5;
    case StateLampState::Working: return 4;
    case StateLampState::Completed: return 3;
    case StateLampState::Offline: return 2;
    case StateLampState::Idle: return 1;
  }
  return 0;
}

String multiAgentUrl() {
#ifdef BRIDGE_AGENTS_URL
  return String(BRIDGE_AGENTS_URL);
#else
  String url(BRIDGE_STATUS_URL);
  if (url.endsWith("/status")) {
    url.remove(url.length() - strlen("/status"));
    url += "/agents";
  }
  return url;
#endif
}

String responseMessage(const JsonDocument& document) {
  if (!document["message"].is<const char*>()) return String();
  return String(document["message"].as<const char*>());
}

void renderIfChanged(StateLampState next, const String& message = String()) {
  const StateLampState previous = StateLamp.state();
  if (previous == next && lastMessage == message) return;
  StateLamp.setState(next);
  lastMessage = message;
  lastOverviewSignature = String();
  core2_ui::render(next, message);
  if (previous != next) core2_audio::onStateTransition(next, millis());
  Serial.printf(
      "core2-wifi: display state=%s, message_bytes=%u, transitioned=%s\n",
      StateLamp.stateName(), message.length(), previous != next ? "yes" : "no");
}

void connectWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED ||
      now - lastWifiAttemptAt < kWifiRetryIntervalMs) {
    return;
  }
  lastWifiAttemptAt = now;
  Serial.printf("wifi: connecting ssid=%s\n", WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void pollBridge(uint32_t now) {
  connectWifi(now);
  if (now - lastPollAt < kPollIntervalMs) return;
  lastPollAt = now;

  if (WiFi.status() != WL_CONNECTED) {
    renderIfChanged(StateLampState::Offline);
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(agentsUrl)) {
    renderIfChanged(StateLampState::Offline);
    return;
  }

  const int code = http.GET();
  if (code == HTTP_CODE_NOT_FOUND) {
    // Staged rollout: an old v1.2 Bridge still provides the single-agent view.
    http.end();
    if (!http.begin(BRIDGE_STATUS_URL)) {
      renderIfChanged(StateLampState::Offline);
      return;
    }
    const int fallbackCode = http.GET();
    if (fallbackCode != HTTP_CODE_OK) {
      http.end();
      renderIfChanged(StateLampState::Offline);
      return;
    }
    JsonDocument fallback;
    const DeserializationError fallbackError =
        deserializeJson(fallback, http.getStream());
    http.end();
    if (fallbackError || !fallback["state"].is<const char*>()) {
      renderIfChanged(StateLampState::Offline);
      return;
    }
    StateLampState parsed;
    if (!StateLamp.parseState(fallback["state"], parsed)) {
      renderIfChanged(StateLampState::Error);
      return;
    }
    renderIfChanged(parsed, responseMessage(fallback));
    return;
  }
  if (code != HTTP_CODE_OK) {
    Serial.printf("wifi: bridge HTTP status=%d\n", code);
    http.end();
    renderIfChanged(StateLampState::Offline);
    return;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, http.getStream());
  http.end();
  if (error || !document.is<JsonArray>()) {
    Serial.printf("wifi: invalid Bridge response=%s\n", error.c_str());
    renderIfChanged(StateLampState::Offline);
    return;
  }

  agentCount = 0;
  size_t primaryIndex = 0;
  uint8_t primaryPriority = 0;
  for (JsonObject item : document.as<JsonArray>()) {
    if (agentCount >= kMaxAgents || !item["agent"].is<const char*>() ||
        !item["state"].is<const char*>()) {
      continue;
    }
    StateLampState parsed;
    if (!StateLamp.parseState(item["state"], parsed)) continue;
    agents[agentCount] = {String(item["agent"].as<const char*>()), parsed};
    const uint8_t candidatePriority = priority(parsed);
    if (candidatePriority > primaryPriority) {
      primaryPriority = candidatePriority;
      primaryIndex = agentCount;
    }
    ++agentCount;
  }
  if (agentCount == 0) {
    renderIfChanged(StateLampState::Offline);
    return;
  }
  const StateLampState previous = StateLamp.state();
  const StateLampState selected = agents[primaryIndex].state;
  String signature;
  for (size_t index = 0; index < agentCount; ++index) {
    signature += agents[index].agent;
    signature += ':';
    signature += String(priority(agents[index].state));
    signature += ';';
  }
  signature += String(primaryIndex);
  StateLamp.setState(selected);
  if (signature != lastOverviewSignature) {
    lastOverviewSignature = signature;
    core2_ui::renderOverview(agents, agentCount, primaryIndex);
  }
  if (previous != selected) core2_audio::onStateTransition(selected, millis());
  Serial.printf("core2-wifi: overview agents=%u, primary=%s, state=%s\n",
                agentCount, agents[primaryIndex].agent.c_str(), StateLamp.stateName());
}
}  // namespace

void setup() {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.clear_display = false;
  config.output_power = false;
  config.internal_imu = true;
  config.internal_rtc = false;
  config.internal_spk = true;
  config.internal_mic = false;
  M5.begin(config);

  Serial.println();
  Serial.println("StateLamp Core2 Wi-Fi");
  Serial.println("scope: completed C5 UI/audio/local mute + Bridge polling");
  StateLamp.begin();
  core2_ui::begin();
  core2_ui::setDisplayRotation(3);
  core2_ui::render(StateLamp.state());
  core2_audio::begin();
  core2_touch::begin();
  core2_orientation::begin(millis());

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  lastWifiAttemptAt = millis() - kWifiRetryIntervalMs;
  lastPollAt = millis() - kPollIntervalMs;
  lastHeartbeatAt = millis();
  agentsUrl = multiAgentUrl();
  Serial.printf("wifi: Bridge=%s\n", BRIDGE_STATUS_URL);
  Serial.printf("wifi: Agents=%s\n", agentsUrl.c_str());
  Serial.println("core2-wifi: ready (audio unmuted, no startup tone)");
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  if (core2_touch::rotationLockToggleRequested()) {
    const bool locked = !core2_orientation::isLocked();
    core2_orientation::setLocked(locked, now);
    core2_ui::setRotationLocked(locked);
    core2_ui::showPresentationOverlay(locked ? "Rotation locked" : "Auto rotation",
                                      now);
  }
  if (core2_touch::muteToggleRequested()) {
    core2_audio::setMuted(!core2_audio::isMuted());
    core2_ui::setAudioMuted(core2_audio::isMuted());
  }
  if (core2_orientation::tick(now)) {
    core2_ui::setDisplayRotation(core2_orientation::committedRotation());
  }
  pollBridge(now);
  core2_ui::tick(now);
  core2_audio::tick(now);

  if (now - lastHeartbeatAt >= kHeartbeatIntervalMs) {
    lastHeartbeatAt = now;
    Serial.printf(
        "core2-wifi: alive, wifi=%s, state=%s, audio=%s, muted=%s, "
        "uptime_ms=%u, free_heap=%u\n",
        WiFi.status() == WL_CONNECTED ? "connected" : "offline",
        StateLamp.stateName(), core2_audio::isActive() ? "active" : "idle",
        core2_audio::isMuted() ? "yes" : "no", now, ESP.getFreeHeap());
  }
  delay(10);
}
