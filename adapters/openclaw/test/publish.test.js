import assert from "node:assert/strict";
import test from "node:test";

import { createBridgePublisher } from "../publish.js";

const status = {
  agent: "openclaw",
  state: "working",
  message: "OpenClaw is working",
};

test("posts normalized status to the configured Bridge URL", async () => {
  let request;
  const publish = createBridgePublisher({
    bridgeUrl: "http://bridge.test/api/v1/status",
    timeoutMs: 500,
    logger: {},
    fetchImpl: async (url, options) => {
      request = { url, options };
      return { ok: true, status: 200 };
    },
  });

  assert.equal(await publish(status), true);
  assert.equal(request.url, "http://bridge.test/api/v1/status");
  assert.equal(request.options.method, "POST");
  assert.equal(request.options.headers["content-type"], "application/json");
  assert.deepEqual(JSON.parse(request.options.body), status);
  assert.ok(request.options.signal instanceof AbortSignal);
});

test("HTTP failure is reported but does not reject the OpenClaw hook", async () => {
  const warnings = [];
  const publish = createBridgePublisher({
    bridgeUrl: "http://bridge.test/api/v1/status",
    timeoutMs: 500,
    logger: { warn: (message) => warnings.push(message) },
    fetchImpl: async () => ({ ok: false, status: 503 }),
  });

  assert.equal(await publish(status), false);
  assert.match(warnings[0], /HTTP 503/);
});

test("connection failure is swallowed so OpenClaw can continue", async () => {
  const warnings = [];
  const publish = createBridgePublisher({
    bridgeUrl: "http://bridge.test/api/v1/status",
    timeoutMs: 500,
    logger: { warn: (message) => warnings.push(message) },
    fetchImpl: async () => {
      throw new Error("connection refused");
    },
  });

  await assert.doesNotReject(() => publish(status));
  assert.match(warnings[0], /connection refused/);
});
