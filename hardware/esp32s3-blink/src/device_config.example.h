#pragma once

// Copy this file to device_config.h and edit for your LAN.
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define BRIDGE_STATUS_URL "http://127.0.0.1:18480/api/v1/status"
// Optional. When omitted, core2-wifi derives /api/v1/agents from the status URL.
// #define BRIDGE_AGENTS_URL "http://127.0.0.1:18480/api/v1/agents"

// Used only by the BLE build.
#define BLE_DEVICE_NAME "StateLamp"
