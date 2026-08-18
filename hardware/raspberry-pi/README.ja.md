# Raspberry Pi用Agent Team Console

このディレクトリには、Raspberry Pi上で動作するStateLampのチームコンソールを収録しています。共有Bridgeをpollする独立したクライアントで、複数エージェントの状態、人間へのattention要求、Linuxホストの状態を800×480画面に表示します。

## 対応構成

- Raspberry Pi 4または互換のRaspberry Pi
- 800×480 HDMIディスプレイ
- 必要に応じてXPT2046抵抗膜タッチコントローラ

このUI専用の項目をBridgeプロトコルへ追加せず、既存のagent-agnostic APIだけを使用します。マシン固有のホスト名、IPアドレス、認証情報はリポジトリへ保存しないでください。

## 開発時の起動

```bash
python3 hardware/raspberry-pi/console/server.py \
  --bridge-url http://127.0.0.1:18480
```

ブラウザで `http://127.0.0.1:18880` を開きます。環境変数 `STATELAMP_BRIDGE_URL` でBridge URLを指定することもできます。UIは2秒ごとにpollし、Bridgeとの切断を明示表示します。状態遷移履歴はブラウザのローカルストレージに保存されますが、これは表示上の便利機能であり、正式なイベント履歴ではありません。

## テスト

```bash
python3 -m unittest -v hardware/raspberry-pi/console/test_server.py
node hardware/raspberry-pi/console/test_console_state.mjs
```

本番ではCageとChromiumによるkiosk表示を想定しています。開発UIの動作確認と再起動後の復旧を確認するまで、パッケージの追加やsystemdサービスの有効化は行わないでください。systemd、kiosk、タッチ設定の詳細は英語版READMEを参照してください。

## Boot services

付属のsystemd unitは、StateLampを`/opt/statelamp`へ配置し、そこに
`bridge/.venv`を作成済みであることを前提にしています。次の例では、リポジトリを
配置し、Bridge用venvへrequirementsをインストールしてから、template unitを登録します。
`YOUR_USER`はcheckoutを所有するLinuxアカウント名に置き換えてください。ユーザー名やUIDはunitへ固定していません。

```bash
sudo install -d /opt/statelamp
sudo cp -a . /opt/statelamp
python3 -m venv /opt/statelamp/bridge/.venv
/opt/statelamp/bridge/.venv/bin/python -m pip install -r /opt/statelamp/bridge/requirements.txt
sudo install -m 0644 hardware/raspberry-pi/systemd/statelamp-bridge@.service /etc/systemd/system/
sudo install -m 0644 hardware/raspberry-pi/systemd/statelamp-console@.service /etc/systemd/system/
sudo install -m 0644 hardware/raspberry-pi/systemd/statelamp-kiosk@.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now statelamp-bridge@YOUR_USER.service statelamp-console@YOUR_USER.service statelamp-kiosk@YOUR_USER.service
```
