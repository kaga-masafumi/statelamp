#pragma once

// Copy this file to device_config.h and edit for your LAN.
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
#define BRIDGE_STATUS_URL "http://your-bridge-host:8000/api/v1/status"
// Optional. When omitted, core2-wifi derives /api/v1/agents from the status URL.
// #define BRIDGE_AGENTS_URL "http://your-bridge-host:8000/api/v1/agents"

// Used only by the BLE build.
#define BLE_DEVICE_NAME "StateLamp"
