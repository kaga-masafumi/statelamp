# ESP32-S3: StateLamp state LED version

日本語版: [README.ja.md](README.ja.md)

This is the StateLamp ESP32-S3 Wi-Fi version. It polls the Bridge and uses
the onboard RGB LED on GPIO48 to show the agent state with different colors
and blink patterns.

Copy `src/device_config.example.h` to `src/device_config.h`, set the local
Wi-Fi values and Bridge URL, then upload:

```bash
cp hardware/esp32s3-blink/src/device_config.example.h \
  hardware/esp32s3-blink/src/device_config.h
pio run -d hardware/esp32s3-blink -t upload
```
