# StateLamp HTTP protocol

日本語版: [protocol.ja.md](protocol.ja.md)

The Bridge accepts normalized, agent-agnostic status updates. The service is
in-memory and unauthenticated in this release; keep it on a trusted network.

## Status

```http
GET /api/v1/status
```

```json
{"agent":"example-agent","state":"working","message":"Running tests"}
```

Publish a status with:

```http
POST /api/v1/status
Content-Type: application/json
```

```json
{"agent":"example-agent","state":"completed","message":"Tests passed"}
```

States are `idle`, `working`, `waiting_approval`, `human_required`,
`completed`, and `error`. `offline` is a device-local state when a device
cannot reach the Bridge.

## Human attention

Create an attention request:

```http
POST /api/v1/attention
```

```json
{
  "agent":"example-agent",
  "reason":"user_input",
  "message":"Choose the deployment target",
  "origin_session_id":"session-example",
  "run_id":"run-example"
}
```

The response contains an exact attention ID. Clear that ID only after the
request is complete:

```http
POST /api/v1/attention/clear
```

```json
{"id":"attention-id-from-create-response"}
```

Requests expire after the configured TTL. Adapters can cancel their own
requests with `POST /api/v1/attention/cancel`, supplying an owner session or
run ID. The Bridge also exposes `GET /api/v1/attention` and
`GET /api/v1/attention/history` for local observability.
