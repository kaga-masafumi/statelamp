#!/usr/bin/env node

import { fileURLToPath } from "node:url";
import { randomUUID } from "node:crypto";

import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

import {
  ATTENTION_REASONS,
  createHumanCallClient,
  humanCallToolResult,
  resolveAttentionUrl,
} from "../../shared/human-call.js";

export const SERVER_INSTRUCTIONS = [
  "Use human_required only when progress genuinely requires physical work, visual confirmation, user input, a manual action, or human judgement.",
  "After creating attention, repeat the same request in the conversation, retain attention_id, and wait for explicit human confirmation.",
  "After confirmation, call human_required_clear with exactly that attention_id; never infer an ID or clear another request.",
  "If StateLamp notification fails, the human action is still required and must remain visible in the conversation.",
  "Do not substitute Human Call for Codex platform approvals.",
].join(" ");

export function createCodexMcpServer({ humanCall, originSessionId = `codex-mcp:${randomUUID()}` }) {
  const server = new McpServer(
    { name: "statelamp", version: "0.5.0" },
    { instructions: SERVER_INSTRUCTIONS },
  );

  // Stdio MCP has no portable Codex run-end event. The MCP process/connection
  // is nevertheless a reliable ownership boundary for normal session close.
  // The Bridge enforces the owner fields, so this can never cancel another
  // Codex session's attention.
  let cancelPromise;
  const cancelOwnedAttention = () => {
    if (!cancelPromise) {
      cancelPromise = Promise.resolve(
        humanCall.cancelOwned?.({ origin_session_id: originSessionId }),
      );
    }
    return cancelPromise;
  };
  server.server.onclose = () => {
    void cancelOwnedAttention();
  };

  server.registerTool(
    "human_required",
    {
      title: "Call a human",
      description:
        "Create a StateLamp Human Required request. Repeat the request in conversation, retain attention_id, and wait even when notification fails.",
      inputSchema: {
        reason: z.enum(ATTENTION_REASONS).describe("Why a human is required"),
        message: z.string().min(1).max(500).describe("Exact action to show the human"),
      },
      annotations: { readOnlyHint: false, destructiveHint: false, idempotentHint: false },
    },
    async ({ reason, message }) =>
      humanCallToolResult(
        await humanCall.request({
          agent: "codex",
          reason,
          message,
          origin_session_id: originSessionId,
        }),
      ),
  );

  server.registerTool(
    "human_required_clear",
    {
      title: "Clear one human call",
      description:
        "After explicit human confirmation, clear exactly the supplied StateLamp attention ID.",
      inputSchema: {
        attention_id: z.string().min(1).max(100).describe("Exact ID returned by human_required"),
      },
      annotations: { readOnlyHint: false, destructiveHint: true, idempotentHint: true },
    },
    async ({ attention_id }) =>
      humanCallToolResult(
        await humanCall.clear(attention_id, { origin_session_id: originSessionId }),
      ),
  );

  // Exposed for main() so stdin/SIGTERM shutdown can await the best-effort
  // cancel before the short-lived stdio process exits.
  server.cancelOwnedAttention = cancelOwnedAttention;

  return server;
}

export function createConfiguredHumanCallClient(env = process.env) {
  const bridgeUrl = env.STATELAMP_URL ?? "http://127.0.0.1:18480/api/v1/status";
  const parsedTimeout = Number.parseInt(env.STATELAMP_TIMEOUT_MS ?? "1500", 10);
  const timeoutMs = Number.isInteger(parsedTimeout) && parsedTimeout > 0 ? parsedTimeout : 1500;
  return createHumanCallClient({
    attentionUrl: resolveAttentionUrl(bridgeUrl, env.STATELAMP_ATTENTION_URL),
    timeoutMs,
    logger: { warn: (message) => console.error(message) },
  });
}

export async function main() {
  const humanCall = createConfiguredHumanCallClient();
  const server = createCodexMcpServer({ humanCall });
  const transport = new StdioServerTransport();
  let shuttingDown = false;
  const shutdown = async () => {
    if (shuttingDown) return;
    shuttingDown = true;
    await server.close().catch(() => {});
    await server.cancelOwnedAttention();
  };
  process.stdin.once("end", () => void shutdown());
  process.stdin.once("close", () => void shutdown());
  process.once("SIGTERM", () => void shutdown());
  process.once("SIGINT", () => void shutdown());
  await server.connect(transport);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === fileURLToPath(new URL(`file://${process.argv[1]}`))) {
  main().catch((error) => {
    console.error(`statelamp MCP failed: ${String(error)}`);
    process.exitCode = 1;
  });
}
