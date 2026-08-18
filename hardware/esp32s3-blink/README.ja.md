# ESP32-S3：StateLamp状態LED版

ESP32-S3 SuperMini系ボードのオンボードRGB LED（GPIO48）を使い、Bridgeから取得したエージェント状態を色と点滅パターンで表示します。`working`、`waiting_approval`、`human_required`、`completed`、`error`、`offline`を区別できます。

`src/device_config.example.h`を`src/device_config.h`へコピーし、ローカルのWi-Fi情報とBridge URLを設定してください。

```bash
cp hardware/esp32s3-blink/src/device_config.example.h \
  hardware/esp32s3-blink/src/device_config.h
pio run -d hardware/esp32s3-blink -t upload
```
