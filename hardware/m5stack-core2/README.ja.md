# M5Stack Core2：StateLamp端末

Core2はWi-Fi経由でStateLamp Bridgeをpollし、複数エージェントのうち最も優先度の高い状態を画面に表示します。重要な状態遷移では通知音を鳴らし、タッチ操作でローカルの通知音muteを切り替えられます。

## セットアップ

1. [PlatformIO](https://platformio.org/)とESP32プラットフォームをインストールします。
2. ローカル設定ファイルをコピーします。

   ```bash
   cp hardware/m5stack-core2/include/device_config.example.h \
      hardware/m5stack-core2/include/device_config.h
   ```

3. `device_config.h`を編集し、`WIFI_SSID`、`WIFI_PASSWORD`、`BRIDGE_STATUS_URL`を設定します。Core2から到達できるBridgeのアドレスを指定してください。`127.0.0.1`はCore2自身を指すため使用できません。
4. Core2をUSB接続して書き込みます。

   ```bash
   pio run -d hardware/m5stack-core2 -e core2-wifi -t upload
   pio device monitor -d hardware/m5stack-core2
   ```

BridgeのURLから`/api/v1/agents`を導出して複数エージェントの状態を取得します。このリリースのBridgeには認証がないため、信頼できるネットワーク内で使用してください。

## ローカル操作

Core2のタッチ操作はローカル状態だけを変更します。設定された長押し操作で通知音のmuteを切り替えますが、Bridgeが管理するエージェント状態やattentionの所有権は変更しません。
