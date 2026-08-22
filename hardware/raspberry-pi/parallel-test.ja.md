# Raspberry Pi の並行テスト配置

この手順は、既存の本番 StateLamp を止めずに、同じ Raspberry Pi 上でローカル専用のテスト Bridge と Console を動かします。テスト側は `127.0.0.1:28000` と `127.0.0.1:28880` だけを使用し、本番の `18480` と `18880`、および本番サービスの状態を共有しません。

以下は checkout を所有する通常ユーザーで実行します。`<REPOSITORY_URL>` は StateLamp リポジトリの URL に置き換えてください。

## 配置と Bridge 依存関係

```bash
git clone <REPOSITORY_URL> ~/statelamp-test
python3 -m venv ~/statelamp-test/bridge/.venv
~/statelamp-test/bridge/.venv/bin/python -m pip install -r ~/statelamp-test/bridge/requirements.txt
```

Console は Python 標準ライブラリだけを使うため、追加の Console 用依存関係はインストールしません。

## user service の登録

```bash
mkdir -p ~/.config/systemd/user
install -m 0644 ~/statelamp-test/hardware/raspberry-pi/systemd/statelamp-test-bridge.service ~/.config/systemd/user/
install -m 0644 ~/statelamp-test/hardware/raspberry-pi/systemd/statelamp-test-console.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now statelamp-test-bridge.service statelamp-test-console.service
```

`statelamp-test-console.service` は `statelamp-test-bridge.service` を必要とし、先に起動します。どちらの unit も user service なので、既存の本番 system service を変更しません。

## 確認

```bash
curl --fail http://127.0.0.1:28000/healthz
curl --fail http://127.0.0.1:28880/healthz
systemctl --user --no-pager status statelamp-test-bridge.service statelamp-test-console.service
ss -ltn '( sport = :28000 or sport = :28880 or sport = :18480 or sport = :18880 )'
```

前の二つはテストの health endpoint です。`ss` の出力でテストが `127.0.0.1:28000` と `127.0.0.1:28880`、本番が別の `18480` と `18880` を使っていることを確認します。

## 停止と片付け

```bash
systemctl --user disable --now statelamp-test-console.service statelamp-test-bridge.service
rm -f ~/.config/systemd/user/statelamp-test-console.service ~/.config/systemd/user/statelamp-test-bridge.service
systemctl --user daemon-reload
```

この片付けは user unit と実行中のテストサービスだけを対象にします。`~/statelamp-test` は自動削除しないため、必要に応じて内容を確認してから手動で扱ってください。
