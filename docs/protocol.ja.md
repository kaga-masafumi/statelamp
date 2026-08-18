# StateLamp HTTPプロトコル

Bridgeは、エージェントに依存しない正規化済みの状態更新を受け付けます。現在の実装はメモリ上で動作し、認証もありません。信頼できるプライベートネットワーク内で使用してください。

## 状態

```http
GET /api/v1/status
```

```json
{"agent":"example-agent","state":"working","message":"テストを実行中"}
```

状態を送信するには、次のようにします。

```http
POST /api/v1/status
Content-Type: application/json
```

```json
{"agent":"example-agent","state":"completed","message":"テスト完了"}
```

状態は `idle`、`working`、`waiting_approval`、`human_required`、`completed`、`error` です。`offline` はデバイスがBridgeへ接続できない場合にデバイス側で表示する状態です。

## 人間への注意要求

注意要求を作成します。

```http
POST /api/v1/attention
```

```json
{
  "agent":"example-agent",
  "reason":"user_input",
  "message":"デプロイ先を選択してください",
  "origin_session_id":"session-example",
  "run_id":"run-example"
}
```

レスポンスには一意のattention IDが含まれます。要求が完了した後、そのIDだけを解除してください。

```http
POST /api/v1/attention/clear
```

```json
{"id":"作成時のレスポンスに含まれるattention ID"}
```

要求は設定されたTTLで期限切れになります。Adapterは、所有するsession IDまたはrun IDを指定して `POST /api/v1/attention/cancel` から自分の要求を取り消せます。ローカルで状態を観測するため、`GET /api/v1/attention` と `GET /api/v1/attention/history` も利用できます。
