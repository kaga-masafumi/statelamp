# M5Stack Core2: StateLamp endpoint

日本語版: [README.ja.md](README.ja.md)

The Core2 firmware polls the StateLamp Bridge over Wi-Fi. It shows the
highest-priority agent state on the display, plays notification sounds for
important transitions, and supports local mute control.

## Setup

1. Install [PlatformIO](https://platformio.org/) and the ESP32 platform.
2. Copy the local configuration template:

   ```bash
   cp hardware/m5stack-core2/include/device_config.example.h \
      hardware/m5stack-core2/include/device_config.h
   ```

3. Edit `device_config.h` and set `WIFI_SSID`, `WIFI_PASSWORD`, and
   `BRIDGE_STATUS_URL`. Use an address reachable from the Core2, not
   `127.0.0.1`.
4. Connect the Core2 by USB and upload:

   ```bash
   pio run -d hardware/m5stack-core2 -e core2-wifi -t upload
   pio device monitor -d hardware/m5stack-core2
   ```

The firmware polls `/api/v1/agents` derived from the status URL. Keep the
Bridge on a trusted network; this release does not provide authentication.

## Local controls

The Core2 touch controls are local only. The configured long-press action
toggles notification audio mute; Bridge state and attention ownership remain
unchanged.
