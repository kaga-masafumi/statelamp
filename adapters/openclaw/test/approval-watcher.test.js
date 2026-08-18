import assert from "node:assert/strict";
import test from "node:test";

import { ApprovalWatcher } from "../approval-watcher.js";
import { RunStateCoordinator } from "../state.js";

const flush = () => new Promise((resolve) => setImmediate(resolve));

function createHarness({ execPending = [], pluginPending = [], publish } = {}) {
  let callbacks;
  const published = [];
  const requests = [];
  const client = {
    async request(method) {
      requests.push(method);
      if (method === "exec.approval.list") return execPending;
      if (method === "plugin.approval.list") return pluginPending;
      throw new Error(`unexpected method: ${method}`);
    },
    async stopAndWait() {},
    stop() {},
  };
  const watcher = new ApprovalWatcher({
    coordinator: new RunStateCoordinator(),
    logger: {},
    publish: publish ?? (async (status) => published.push(status)),
    createClient: async (value) => {
      callbacks = value;
      return client;
    },
    startClient: async () => {
      callbacks.onHelloOk();
      return { ready: true };
    },
  });
  return { watcher, client, published, requests, get callbacks() { return callbacks; } };
}

test("backfills exec and plugin approvals after connecting", async () => {
  const harness = createHarness({
    execPending: [{ id: "exec-1" }],
    pluginPending: [{ id: "plugin:1" }],
  });

  await harness.watcher.start();
  await flush();

  assert.deepEqual(harness.requests, ["exec.approval.list", "plugin.approval.list"]);
  assert.equal(harness.published.at(-1).state, "waiting_approval");
  assert.match(harness.published.at(-1).message, /^2 approval/);
});

test("handles both approval event families and resolves only when empty", async () => {
  const harness = createHarness();
  await harness.watcher.start();
  await flush();

  harness.callbacks.onEvent({ event: "exec.approval.requested", payload: { id: "exec-1" } });
  harness.callbacks.onEvent({ event: "plugin.approval.requested", payload: { id: "plugin:1" } });
  harness.callbacks.onEvent({ event: "exec.approval.resolved", payload: { id: "exec-1" } });
  await flush();
  assert.equal(harness.published.at(-1).state, "waiting_approval");

  harness.callbacks.onEvent({ event: "plugin.approval.resolved", payload: { id: "plugin:1" } });
  await flush();
  assert.equal(harness.published.at(-1).state, "idle");
});

test("replays events arriving while reconnect backfill is in flight", async () => {
  let releaseList;
  const listGate = new Promise((resolve) => { releaseList = resolve; });
  const harness = createHarness();
  harness.client.request = async () => {
    await listGate;
    return [{ id: "already-resolved" }];
  };

  await harness.watcher.start();
  harness.callbacks.onEvent({
    event: "exec.approval.resolved",
    payload: { id: "already-resolved" },
  });
  harness.callbacks.onEvent({
    event: "plugin.approval.requested",
    payload: { id: "plugin:new" },
  });
  releaseList();
  await flush();
  await flush();

  assert.equal(harness.published.at(-1).state, "waiting_approval");
  assert.match(harness.published.at(-1).message, /^1 approval/);
});

test("publisher failure is swallowed", async () => {
  const harness = createHarness({
    publish: async () => {
      throw new Error("Bridge unavailable");
    },
  });

  await assert.doesNotReject(() => harness.watcher.start());
  harness.callbacks.onEvent({ event: "exec.approval.requested", payload: { id: "exec-1" } });
  await flush();
});

test("client startup failure does not reject Gateway service start", async () => {
  const watcher = new ApprovalWatcher({
    coordinator: new RunStateCoordinator(),
    logger: {},
    publish: async () => {},
    createClient: async () => {
      throw new Error("client unavailable");
    },
    startClient: async () => ({ ready: false }),
  });

  await assert.doesNotReject(() => watcher.start());
});
