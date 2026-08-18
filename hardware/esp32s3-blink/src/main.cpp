#include <Arduino.h>

#include "statelamp_state.h"
#include "device_config.h"

#if (defined(STATELAMP_TRANSPORT_WIFI) + defined(STATELAMP_TRANSPORT_USB) + \
     defined(STATELAMP_TRANSPORT_BLE)) != 1
#error "Define exactly one STATELAMP_TRANSPORT_*"
#endif

#if defined(STATELAMP_TRANSPORT_WIFI)
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#elif defined(STATELAMP_TRANSPORT_BLE)
#include "ble_gatt_adapter.h"
#endif

namespace {
constexpr uint8_t LED_PIN = 48;
constexpr uint8_t LED_BRIGHTNESS = 32;
StateLampController StateLamp(LED_PIN, LED_BRIGHTNESS);

#if defined(STATELAMP_TRANSPORT_WIFI)
constexpr uint32_t POLL_INTERVAL_MS = 1000;
constexpr uint32_t HTTP_TIMEOUT_MS = 750;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 5000;
uint32_t lastPollAt = 0;
uint32_t lastWifiAttemptAt = 0;

void connectWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED ||
      now - lastWifiAttemptAt < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiAttemptAt = now;
  Serial.printf("Connecting to Wi-Fi '%s'...\n", WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void serviceTransport(uint32_t now) {
  connectWifi(now);
  if (now - lastPollAt < POLL_INTERVAL_MS) return;
  lastPollAt = now;
  if (WiFi.status() != WL_CONNECTED) {
    StateLamp.setState(StateLampState::Offline);
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(BRIDGE_STATUS_URL)) {
    StateLamp.setState(StateLampState::Offline);
    return;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    StateLamp.setState(StateLampState::Offline);
    return;
  }
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, http.getStream());
  http.end();
  if (error || !document["state"].is<const char *>()) {
    StateLamp.setState(StateLampState::Offline);
    return;
  }
  StateLampState parsed;
  if (!StateLamp.parseState(document["state"], parsed)) {
    StateLamp.setState(StateLampState::Error);
    return;
  }
  StateLamp.setState(parsed);
}

void beginTransport() {
  WiFi.mode(WIFI_STA);
  lastWifiAttemptAt = millis() - WIFI_RETRY_INTERVAL_MS;
  lastPollAt = millis() - POLL_INTERVAL_MS;
  Serial.println("Transport: Wi-Fi");
}

#elif defined(STATELAMP_TRANSPORT_USB)
constexpr size_t USB_COMMAND_MAX = 512;
char commandBuffer[USB_COMMAND_MAX + 1];
size_t commandLength = 0;
bool discardingOversizedCommand = false;

void respondToCommand() {
  commandBuffer[commandLength] = '\0';
  String response;
  StateLamp.applyJson(commandBuffer, commandLength, response);
  Serial.println(response);
  commandLength = 0;
}

void serviceTransport(uint32_t) {
  while (Serial.available()) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r') continue;
    if (value == '\n') {
      if (discardingOversizedCommand) {
        discardingOversizedCommand = false;
        commandLength = 0;
      } else if (commandLength > 0) {
        respondToCommand();
      }
      continue;
    }
    if (discardingOversizedCommand) continue;
    if (commandLength >= USB_COMMAND_MAX) {
      discardingOversizedCommand = true;
      commandLength = 0;
      Serial.println("{\"ok\":false,\"error\":\"command_too_long\"}");
      continue;
    }
    commandBuffer[commandLength++] = value;
  }
}

void beginTransport() { Serial.println("Transport: USB CDC"); }

#elif defined(STATELAMP_TRANSPORT_BLE)
#ifndef BLE_DEVICE_NAME
#define BLE_DEVICE_NAME "StateLamp"
#endif

bool handleBleCommand(void* context, const char* payload, size_t length,
                      String& response) {
  auto& controller = *static_cast<StateLampController*>(context);
  return controller.applyJson(payload, length, response);
}

BleGattAdapter ble(&StateLamp, handleBleCommand, "ble");

void beginTransport() {
  ble.begin(BLE_DEVICE_NAME);
  Serial.printf("Transport: BLE (%s)\n", BLE_DEVICE_NAME);
}

void serviceTransport(uint32_t) {}
#endif
}  // namespace

void setup() {
#if defined(STATELAMP_TRANSPORT_USB)
  Serial.setRxBufferSize(1024);
#endif
  Serial.begin(115200);
  StateLamp.begin();
  beginTransport();
  Serial.println("StateLamp starting");
}

void loop() {
  const uint32_t now = millis();
  serviceTransport(now);
  StateLamp.update(now);
  delay(5);
}
