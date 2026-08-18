import { definePluginEntry } from "openclaw/plugin-sdk/plugin-entry";
import {
  createOperatorApprovalsGatewayClient,
  startGatewayClientWhenEventLoopReady,
} from "openclaw/plugin-sdk/gateway-runtime";

import { ApprovalWatcher } from "./approval-watcher.js";
import { resolveConfig } from "./config.js";
import {
  createHumanCallClient,
  humanCallToolResult,
  resolveAttentionUrl,
} from "./human-call.js";
import { createBridgePublisher } from "./publish.js";
import { RunStateCoordinator } from "./state.js";

function sessionIdFromContext(ctx: any): string | undefined {
  const value = ctx?.sessionManager?.sessionId;
  return typeof value === "string" && value.length > 0 ? value : undefined;
}

function runIdFromEvent(event: any, ctx: any): string | undefined {
  const value = event?.runId ?? ctx?.runId;
  return typeof value === "string" && value.length > 0 ? value : undefined;
}

export default definePluginEntry({
  id: "statelamp",
  name: "StateLamp",
  description: "Publishes normalized OpenClaw run state to StateLamp Bridge.",
  register(api) {
    const config = resolveConfig(api.pluginConfig);
    const coordinator = new RunStateCoordinator(config.agentId);
    if (config.usedDeprecatedAgentName) {
      api.logger?.warn?.(
        "StateLamp config field agentName is deprecated; use agentId instead",
      );
    }
    let approvalWatcher = null;

    const publish = createBridgePublisher({
      bridgeUrl: config.bridgeUrl,
      timeoutMs: config.timeoutMs,
      logger: api.logger,
    });
    const humanCall = createHumanCallClient({
      attentionUrl: resolveAttentionUrl(config.bridgeUrl, config.attentionUrl),
      timeoutMs: config.timeoutMs,
      logger: api.logger,
    });

    api.registerTool({
      name: "human_required",
      label: "Call a human",
      description:
        "Call a human through StateLamp when physical work, visual confirmation, user input, a manual action, or human judgement is required. Always also tell the human what is needed in the conversation and wait; StateLamp failure does not remove the need.",
      promptSnippet: "Call a human through StateLamp and return an attention ID.",
      promptGuidelines: [
        "Use human_required when progress genuinely depends on physical work, visual confirmation, user input, a manual action, or human judgement.",
        "After calling, state the requested action in the conversation and wait even if StateLamp notification fails.",
        "Retain the returned attention_id and clear only that ID after the human confirms completion.",
        "Do not use human_required for ordinary Gateway approval events; the approval watcher handles those.",
      ],
      parameters: {
        type: "object",
        additionalProperties: false,
        required: ["reason", "message"],
        properties: {
          reason: {
            type: "string",
            enum: ["approval", "physical_check", "user_input", "manual_action", "judgement", "other"],
          },
          message: { type: "string", minLength: 1, maxLength: 500 },
        },
      },
      async execute(_id, params, _signal, _onUpdate, ctx) {
        const origin_session_id = sessionIdFromContext(ctx);
        return humanCallToolResult(
          await humanCall.request({
            agent: config.agentId,
            reason: params.reason,
            message: params.message,
            origin_session_id,
            run_id: coordinator.activeRunId(),
          }),
        );
      },
    });

    api.registerTool({
      name: "human_required_clear",
      label: "Clear a human call",
      description:
        "Clear exactly one StateLamp human call after the human confirms that request is complete. Never clear another pending attention ID.",
      promptSnippet: "Clear one StateLamp attention ID after completion.",
      parameters: {
        type: "object",
        additionalProperties: false,
        required: ["attention_id"],
        properties: {
          attention_id: { type: "string", minLength: 1, maxLength: 100 },
        },
      },
      async execute(_id, params, _signal, _onUpdate, ctx) {
        const origin_session_id = sessionIdFromContext(ctx);
        return humanCallToolResult(
          await humanCall.clear(params.attention_id, { origin_session_id }),
        );
      },
    });

    api.on("gateway_start", async () => {
      await publish(coordinator.ready());
    });

    api.on("llm_input", async (event) => {
      await publish(coordinator.start(event.runId));
    });

    api.on("agent_end", async (event, ctx) => {
      await publish(
        coordinator.end(event.runId ?? ctx.runId, event.success, event.error),
      );
      const origin_session_id = sessionIdFromContext(ctx);
      if (origin_session_id) {
        const run_id = runIdFromEvent(event, ctx);
        await humanCall.cancelOwned({
          origin_session_id,
          ...(run_id ? { run_id } : {}),
        });
      }
    });

    api.registerService({
      id: "statelamp-approval-watcher",
      start: async (ctx) => {
        approvalWatcher = new ApprovalWatcher({
          coordinator,
          publish,
          logger: ctx.logger,
          createClient: (callbacks) =>
            createOperatorApprovalsGatewayClient({
              config: ctx.config,
              clientDisplayName: "StateLamp approval watcher",
              ...callbacks,
            }),
          startClient: (client) =>
            startGatewayClientWhenEventLoopReady(client, {
              clientOptions: {
                preauthHandshakeTimeoutMs: ctx.config.gateway?.handshakeTimeoutMs,
              },
            }),
        });
        await approvalWatcher.start();
      },
      stop: async () => {
        await approvalWatcher?.stop();
        await humanCall.cancelTracked?.();
        approvalWatcher = null;
      },
    });
  },
});
