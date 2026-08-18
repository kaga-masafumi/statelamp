# StateLamp

StateLampは、AIエージェントの状態を物理デバイスに表示するシステムです。ターミナルから離れていても、処理中、承認待ち、人間の入力待ち、完了、エラーなどをひと目で確認できます。

OpenClaw向けに開発され、現在も実際に使用しています。Bridge自体は特定のエージェントに依存しないAPIを使うため、他のエージェントも小さなhookやadapterから接続できます。

StateLampは独立したオープンソースプロジェクトであり、OpenClaw、OpenAI、Anthropicから公式に提供・提携・承認されたものではありません。

## Demo

Raspberry Piのチームコンソールと、M5Stack Core2の状態表示が連携して動作している様子です。

<p>
  <img src="docs/images/statelamp-team-working.jpg" alt="エージェント実行中を表示するStateLamp" width="48%" />
  <img src="docs/images/statelamp-team-completed.jpg" alt="実行完了を表示するStateLamp" width="48%" />
</p>

英語版README: [README.md](README.md)

プロトコル仕様: [docs/protocol.ja.md](docs/protocol.ja.md)
OpenClaw連携: [adapters/openclaw/README.ja.md](adapters/openclaw/README.ja.md)

## なぜStateLampなのか

長時間動作するエージェントは、利用者が別の作業をしている間に動かせることに価値があります。一方、承認、物理的な確認、人間の判断が必要になったときや、処理が完了したときに、端末の画面を見ていないと気付けません。StateLampはエージェントの状態を画面から切り離し、正規化した小さな状態情報を周囲に置けるデバイスへ送ります。

## 仕組み

```mermaid
flowchart LR
  A[AIエージェント] --> I[Hook / adapter / MCP]
  I --> B[StateLamp Bridge]
  B --> T[Wi-Fi / USB / BLE]
  T --> D[物理ビーコン]
```

Bridgeは、メモリ上で状態を保持する小さなFastAPIサービスです。エージェント固有のイベント処理はintegration側に置きます。デバイスがWi-Fi経由でBridgeをpollすることも、ホストがUSBまたはBLEへ中継することもできます。

StateLampはエージェントを制御したり、次に何をすべきかを判断したりするものではありません。正規化された状態とattention要求を受信し、表示するだけです。

## エージェントの状態

| 状態 | 意味 |
|---|---|
| `idle` | 準備完了または非稼働 |
| `working` | runを処理中 |
| `waiting_approval` | 承認判断を待機中 |
| `human_required` | 人間による操作、入力、確認、判断が必要 |
| `completed` | runが正常終了 |
| `error` | runが失敗終了 |
| `offline` | デバイスがBridgeへ接続できないときのデバイス側状態。Bridgeは返さない |

明示的な人間の注意要求は通常の状態とは別に管理されます。各要求にはIDと、`pending`、`resolved`、`cancelled`、`expired`のライフサイクルがあります。これにより、あるエージェントやrunが別の要求を誤って解除することを防ぎます。

## Quick Start: BridgeとGeneric API

Python 3.10以降を推奨します。

```bash
cd bridge
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
uvicorn app.main:app --host 127.0.0.1 --port 8000
```

別のターミナルからテストします。

```bash
curl http://127.0.0.1:8000/healthz
curl -X POST http://127.0.0.1:8000/api/v1/status \
  -H 'content-type: application/json' \
  -d '{"agent":"example-agent","state":"working","message":"Running tests"}'
curl http://127.0.0.1:8000/api/v1/status
```

HTTP APIの完全な仕様は[docs/protocol.md](docs/protocol.md)にあります。現行バージョンのサービスは意図的に認証を持ちません。信頼できるネットワーク内だけで使用し、公開インターネットへ直接公開しないでください。Compose例はデフォルトでlocalhostにbindします。private LAN上の物理デバイスから接続する場合だけ、`STATELAMP_BIND_ADDRESS`を明示的に設定してください。

## OpenClaw integration — Reference implementation

`adapters/openclaw/`は最も完成度が高く、現在も実際に使用しているreference implementationです。次の状態をBridgeへ通知します。

- gateway start → `idle`
- LLM input / run start → `working`
- operator approval event → `waiting_approval`
- エージェントの正常終了 → `completed`
- エージェントの異常終了 → `error`
- 明示的な`human_required` tool call → attention request
- session/run終了 → owner単位のattention cancellation

sessionとrunの所有関係を保持し、attention IDを指定した厳密な解除に対応します。Bridgeへの通知失敗と、人間の操作が依然として必要であることも分離して扱います。インストール方法とOpenClawのバージョン固有の情報は[adapters/openclaw/README.md](adapters/openclaw/README.md)にあります。

## Integrations

| Integration | Status | 範囲 |
|---|---|---|
| OpenClaw | Reference / actively used | Plugin、approval watcher、状態通知、attention lifecycle |
| Generic HTTP API | Available | 正規化された状態またはattentionを直接送信 |
| Codex | Experimental | human call用のlocal stdio MCP server。一般的なCodex runtime integrationとは称していません |
| Claude Code | Planned | 現時点で保守・検証されたadapterは含まれていません |

別のエージェントを接続する場合、通常は小さなhookまたはadapterだけで済みます。

```text
エージェント固有イベント -> 正規化HTTP request -> Bridge -> デバイス
```

## Hardware

StateLampには現在、次の構成が含まれています。

- Raspberry Pi用UI：複数エージェントとattention要求を表示するAgent Team Console
- original M5Stack Core2：表示、通知音、ローカルmute
- ESP32-S3：StateLamp状態LED版

主に保守する実用デバイスはRaspberry Pi用UIとM5Stack Core2です。ESP32-S3には、StateLampの状態をLEDで示す軽量版を収録しています。セットアップ方法は[Raspberry Pi](hardware/raspberry-pi/README.ja.md)、[M5Stack Core2](hardware/m5stack-core2/README.ja.md)、[ESP32-S3](hardware/esp32s3-blink/README.ja.md)を参照してください。

## Core2 firmware

M5Stack Core2用PlatformIO environmentは[hardware/m5stack-core2/platformio.ini](hardware/m5stack-core2/platformio.ini)を参照してください。[hardware/m5stack-core2/include/device_config.example.h](hardware/m5stack-core2/include/device_config.example.h)をignored対象の`device_config.h`へコピーし、exampleのWi-Fi情報とBridge URLを置き換えます。

## プロジェクトの状態と制約

個人開発のオープンソースプロジェクトで、現在も開発中です。中心となるBridge APIとOpenClaw連携が最も成熟しています。ファームウェアにはハードウェア固有の前提があり、Codex連携は実験的です。Bridgeは状態をメモリ上に保持し、認証を持たず、再起動すると保留中のattention要求を失います。これらの制約を許容できる、信頼できるネットワーク内で使用してください。

## License

StateLampのオリジナルコードはMIT Licenseです。[LICENSE](LICENSE)を参照してください。依存ライブラリのライセンスは、それぞれのPlatformIO/npm packageに従います。
