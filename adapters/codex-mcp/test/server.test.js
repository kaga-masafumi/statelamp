import assert from "node:assert/strict";
import test from "node:test";

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { InMemoryTransport } from "@modelcontextprotocol/sdk/inMemory.js";

import { createCodexMcpServer, SERVER_INSTRUCTIONS } from "../server.js";

async function protocolHarness(humanCall) {
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  const server = createCodexMcpServer({ humanCall });
  const client = new Client({ name: "statelamp-test", version: "1.0.0" });
  await server.connect(serverTransport);
  await client.connect(clientTransport);
  return { client, server };
}

test("MCP initialize exposes instructions and discovers exactly two tools", async (t) => {
  const { client, server } = await protocolHarness({ request() {}, clear() {} });
  t.after(async () => { await client.close(); await server.close(); });
  assert.match(SERVER_INSTRUCTIONS, /retain attention_id/);
  const listed = await client.listTools();
  assert.deepEqual(listed.tools.map((tool) => tool.name), [
    "human_required",
    "human_required_clear",
  ]);
});

test("create sends codex reason/message and returns the exact attention ID", async (t) => {
  let received;
  const humanCall = {
    async request(request) {
      received = request;
      return {
        notified: true,
        operation: "create",
        request,
        attention: { id: "attention-codex-1", ...request },
        attention_id: "attention-codex-1",
        pending_count: 1,
      };
    },
    async clear() { throw new Error("unexpected"); },
  };
  const { client, server } = await protocolHarness(humanCall);
  t.after(async () => { await client.close(); await server.close(); });
  const result = await client.callTool({
    name: "human_required",
    arguments: { reason: "physical_check", message: "LEDを確認してください" },
  });
  assert.deepEqual(received, {
    agent: "codex",
    reason: "physical_check",
    message: "LEDを確認してください",
    origin_session_id: received.origin_session_id,
  });
  assert.match(received.origin_session_id, /^codex-mcp:/);
  assert.equal(result.structuredContent.attention_id, "attention-codex-1");
  assert.match(result.content[0].text, /LEDを確認してください/);
});

test("clear passes the explicit ID and this MCP session owner", async (t) => {
  let clearedId;
  let clearedOwner;
  const humanCall = {
    async request() { throw new Error("unexpected"); },
    async clear(id, owner) {
      clearedId = id;
      clearedOwner = owner;
      return {
        notified: true,
        operation: "clear",
        request: { id },
        id,
        cleared: true,
        pending_count: 0,
        status: { agent: "codex", state: "idle", message: "Ready" },
      };
    },
  };
  const { client, server } = await protocolHarness(humanCall);
  t.after(async () => { await client.close(); await server.close(); });
  const result = await client.callTool({
    name: "human_required_clear",
    arguments: { attention_id: "attention-codex-2" },
  });
  assert.equal(clearedId, "attention-codex-2");
  assert.match(clearedOwner.origin_session_id, /^codex-mcp:/);
  assert.equal(result.structuredContent.pending_count, 0);
});

test("invalid reason is rejected by MCP input validation", async (t) => {
  let called = false;
  const { client, server } = await protocolHarness({
    async request() { called = true; },
    async clear() {},
  });
  t.after(async () => { await client.close(); await server.close(); });
  const result = await client.callTool({
    name: "human_required",
    arguments: { reason: "coffee", message: "test" },
  });
  assert.equal(called, false);
  assert.equal(result.isError, true);
});

test("notification failure remains model-readable and server stays alive", async (t) => {
  const request = {
    agent: "codex",
    reason: "manual_action",
    message: "USBを接続してください",
  };
  const { client, server } = await protocolHarness({
    async request() {
      return {
        notified: false,
        human_action_still_required: true,
        operation: "create",
        request,
        error: "Bridge unavailable",
      };
    },
    async clear() {},
  });
  t.after(async () => { await client.close(); await server.close(); });
  const result = await client.callTool({
    name: "human_required",
    arguments: { reason: request.reason, message: request.message },
  });
  assert.equal(result.structuredContent.human_action_still_required, true);
  assert.match(result.content[0].text, /USBを接続してください/);
  assert.equal((await client.listTools()).tools.length, 2);
});

test("MCP connection close cancels only this server's owner", async (t) => {
  const calls = [];
  const humanCall = {
    async request(request) {
      calls.push({ operation: "create", request });
      return {
        notified: true,
        operation: "create",
        request,
        attention: { id: "attention-owner-1", ...request },
        pending_count: 1,
      };
    },
    async clear() { throw new Error("unexpected"); },
    async cancelOwned(request) {
      calls.push({ operation: "cancel", request });
      return { notified: true, operation: "cancel", request, cancelled_ids: ["attention-owner-1"] };
    },
  };
  const { client, server } = await protocolHarness(humanCall);
  t.after(async () => { await client.close(); await server.close(); });

  await client.callTool({
    name: "human_required",
    arguments: { reason: "physical_check", message: "確認" },
  });
  await server.close();

  assert.equal(calls[0].operation, "create");
  assert.equal(calls[1].operation, "cancel");
  assert.equal(calls[1].request.origin_session_id, calls[0].request.origin_session_id);
});
