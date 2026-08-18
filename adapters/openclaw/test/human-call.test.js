import assert from "node:assert/strict";
import test from "node:test";

import {
  createHumanCallClient,
  humanCallToolResult,
  resolveAttentionUrl,
} from "../human-call.js";

function jsonResponse(body, { ok = true, status = 200 } = {}) {
  return { ok, status, json: async () => body };
}

function harness(fetchImpl, timeoutMs = 500) {
  const warnings = [];
  return {
    warnings,
    client: createHumanCallClient({
      attentionUrl: "http://bridge.test/api/v1/attention",
      timeoutMs,
      logger: { warn: (message) => warnings.push(message) },
      fetchImpl,
    }),
  };
}

test("derives the attention URL from the configured status endpoint", () => {
  assert.equal(
    resolveAttentionUrl("http://127.0.0.1:18480/api/v1/status"),
    "http://127.0.0.1:18480/api/v1/attention",
  );
  assert.equal(
    resolveAttentionUrl("http://ignored/status", "http://custom/attention"),
    "http://custom/attention",
  );
});

test("creates human_required with reason/message and returns attention ID", async () => {
  let request;
  const { client } = harness(async (url, options) => {
    request = { url, options };
    return jsonResponse({
      attention: { id: "attention-1", agent: "openclaw", reason: "physical_check", message: "LED" },
      pending_count: 1,
    });
  });

  const result = await client.request({
    agent: "openclaw",
    reason: "physical_check",
    message: "LEDを確認してください",
  });

  assert.equal(result.notified, true);
  assert.equal(result.attention.id, "attention-1");
  assert.equal(result.attention_id, "attention-1");
  assert.equal(request.url, "http://bridge.test/api/v1/attention");
  assert.deepEqual(JSON.parse(request.options.body), {
    agent: "openclaw",
    reason: "physical_check",
    message: "LEDを確認してください",
  });
});

test("clears only the supplied attention ID", async () => {
  let request;
  const { client } = harness(async (url, options) => {
    request = { url, options };
    return jsonResponse({ id: "attention-2", cleared: true, pending_count: 1 });
  });

  const result = await client.clear("attention-2");
  assert.equal(result.notified, true);
  assert.equal(result.cleared, true);
  assert.equal(result.pending_count, 1);
  assert.equal(request.url, "http://bridge.test/api/v1/attention/clear");
  assert.deepEqual(JSON.parse(request.options.body), { id: "attention-2" });
});

test("clear and cancel carry the exact recorded owner", async () => {
  const requests = [];
  const { client } = harness(async (url, options) => {
    const body = JSON.parse(options.body);
    requests.push({ url, body });
    if (url.endsWith("/cancel")) {
      return jsonResponse({ cancelled_ids: ["attention-owned"], pending_count: 0 });
    }
    return jsonResponse({
      attention: { id: "attention-owned", ...body },
      pending_count: 1,
    });
  });

  await client.request({
    agent: "codex",
    reason: "physical_check",
    message: "確認",
    origin_session_id: "codex-mcp:session-a",
    run_id: "run-a",
  });
  await client.clear("attention-owned");
  await client.cancelOwned({ origin_session_id: "codex-mcp:session-a" });

  assert.deepEqual(requests[0].body.origin_session_id, "codex-mcp:session-a");
  assert.deepEqual(requests[1].body, {
    id: "attention-owned",
    origin_session_id: "codex-mcp:session-a",
    run_id: "run-a",
  });
  assert.deepEqual(requests[2].body, { origin_session_id: "codex-mcp:session-a" });
});

test("cancelTracked deduplicates owners and does not cancel legacy requests", async () => {
  const requests = [];
  const { client } = harness(async (url, options) => {
    const body = JSON.parse(options.body);
    requests.push({ url, body });
    if (url.endsWith("/cancel")) {
      return jsonResponse({ cancelled_ids: [body.origin_session_id], pending_count: 0 });
    }
    return jsonResponse({ attention: { id: `attention-${requests.length}`, ...body }, pending_count: 1 });
  });

  await client.request({ reason: "physical_check", message: "legacy" });
  await client.request({
    reason: "physical_check",
    message: "owned-1",
    origin_session_id: "openclaw-session-1",
  });
  await client.request({
    reason: "user_input",
    message: "owned-2",
    origin_session_id: "openclaw-session-1",
    run_id: "run-1",
  });
  await client.cancelTracked();

  assert.deepEqual(
    requests.filter(({ url }) => url.endsWith("/cancel")).map(({ body }) => body),
    [{ origin_session_id: "openclaw-session-1" }],
  );
});

test("HTTP timeout becomes a non-throwing failed notification", async () => {
  const { client, warnings } = harness(
    (_url, options) =>
      new Promise((_resolve, reject) => {
        options.signal.addEventListener("abort", () => reject(options.signal.reason));
      }),
    10,
  );

  const result = await client.request({ reason: "user_input", message: "入力してください" });
  assert.equal(result.notified, false);
  assert.match(result.error, /Timeout|timed out/i);
  assert.equal(warnings.length, 1);
});

test("Bridge unavailable becomes a non-throwing failed notification", async () => {
  const { client } = harness(async () => {
    throw new Error("connection refused");
  });
  await assert.doesNotReject(async () => {
    const result = await client.request({ reason: "manual_action", message: "部品を交換してください" });
    assert.equal(result.notified, false);
    assert.match(result.error, /connection refused/);
  });
});

for (const status of [400, 503]) {
  test(`HTTP ${status} becomes a non-throwing failed notification`, async () => {
    const { client } = harness(async () =>
      jsonResponse({ detail: "failure" }, { ok: false, status }),
    );
    const result = await client.request({ reason: "other", message: "確認してください" });
    assert.equal(result.notified, false);
    assert.equal(result.human_action_still_required, true);
    assert.match(result.error, new RegExp(String(status)));
  });
}

test("malformed JSON becomes a non-throwing failed notification", async () => {
  const { client } = harness(async () => ({
    ok: true,
    status: 200,
    json: async () => { throw new SyntaxError("bad JSON"); },
  }));
  const result = await client.request({ reason: "judgement", message: "判断してください" });
  assert.equal(result.notified, false);
  assert.equal(result.human_action_still_required, true);
  assert.match(result.error, /invalid JSON/);
});

test("invalid reason is rejected before HTTP", async () => {
  let called = false;
  const { client } = harness(async () => {
    called = true;
    return jsonResponse({});
  });
  const result = await client.request({ reason: "invalid", message: "test" });
  assert.equal(result.notified, false);
  assert.match(result.error, /Invalid reason/);
  assert.equal(called, false);
});

test("failure tool result keeps the human request visible", () => {
  const output = humanCallToolResult({
    notified: false,
    error: "Bridge unavailable",
    request: { message: "USBを接続してください" },
  });
  assert.match(output.content[0].text, /FAILED/);
  assert.match(output.content[0].text, /USBを接続してください/);
});

test("create failure explicitly preserves the human requirement", async () => {
  const { client } = harness(async () => {
    throw new Error("connection refused");
  });
  const result = await client.request({
    agent: "openclaw",
    reason: "manual_action",
    message: "USBを接続してください",
  });
  assert.equal(result.human_action_still_required, true);
  assert.deepEqual(result.request, {
    agent: "openclaw",
    reason: "manual_action",
    message: "USBを接続してください",
  });
});
