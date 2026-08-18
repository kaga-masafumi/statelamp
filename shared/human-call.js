export const ATTENTION_REASONS = [
  "approval",
  "physical_check",
  "user_input",
  "manual_action",
  "judgement",
  "other",
];

export function resolveAttentionUrl(bridgeUrl, explicitAttentionUrl) {
  if (explicitAttentionUrl) return explicitAttentionUrl;
  const url = new URL(bridgeUrl);
  url.pathname = url.pathname.replace(/\/api\/v1\/status\/?$/, "/api/v1/attention");
  return url.toString();
}

function failure(operation, request, error) {
  return {
    notified: false,
    human_action_still_required: operation === "create",
    operation,
    request,
    error,
  };
}

async function readJson(response) {
  try {
    return await response.json();
  } catch {
    return null;
  }
}

export function createHumanCallClient({ attentionUrl, timeoutMs, logger, fetchImpl = fetch }) {
  const attentionOwners = new Map();

  function ownerFields({ origin_session_id, run_id } = {}) {
    return {
      ...(origin_session_id ? { origin_session_id } : {}),
      ...(run_id ? { run_id } : {}),
    };
  }

  async function post(url, body, operation) {
    try {
      const response = await fetchImpl(url, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(body),
        signal: AbortSignal.timeout(timeoutMs),
      });
      const data = await readJson(response);
      if (!response.ok) {
        const result = failure(operation, body, `Bridge returned HTTP ${response.status}`);
        logger.warn?.(`statelamp: ${result.error}`);
        return result;
      }
      if (!data || typeof data !== "object") {
        const result = failure(operation, body, "Bridge returned an invalid JSON response");
        logger.warn?.(`statelamp: ${result.error}`);
        return result;
      }
      return { notified: true, operation, request: body, ...data };
    } catch (error) {
      const result = failure(operation, body, String(error));
      logger.warn?.(`statelamp: human call ${operation} failed (${String(error)})`);
      return result;
    }
  }

  return {
    async request({ agent = "openclaw", reason, message, origin_session_id, run_id }) {
      if (!ATTENTION_REASONS.includes(reason)) {
        return failure(
          "create",
          { agent, reason, message, ...ownerFields({ origin_session_id, run_id }) },
          `Invalid reason: ${reason}`,
        );
      }
      const request = {
        agent,
        reason,
        message,
        ...ownerFields({ origin_session_id, run_id }),
      };
      const result = await post(attentionUrl, request, "create");
      if (result.notified && typeof result.attention?.id !== "string") {
        const invalid = failure(
          "create",
          request,
          "Bridge response did not contain attention.id",
        );
        logger.warn?.(`statelamp: ${invalid.error}`);
        return invalid;
      }
      if (result.notified) {
        attentionOwners.set(result.attention.id, ownerFields(request));
        return { ...result, attention_id: result.attention.id };
      }
      return result;
    },

    async clear(attentionId, ownership = {}) {
      const request = {
        id: attentionId,
        ...attentionOwners.get(attentionId),
        ...ownerFields(ownership),
      };
      const result = await post(
        `${attentionUrl.replace(/\/$/, "")}/clear`,
        request,
        "clear",
      );
      if (result.notified && result.cleared) attentionOwners.delete(attentionId);
      return result;
    },

    async cancelOwned({ origin_session_id, run_id, agent } = {}) {
      const request = {
        ...ownerFields({ origin_session_id, run_id }),
        ...(agent ? { agent } : {}),
      };
      if (!request.origin_session_id && !request.run_id) {
        return failure("cancel", request, "Owner identity is required to cancel attention");
      }
      const result = await post(
        `${attentionUrl.replace(/\/$/, "")}/cancel`,
        request,
        "cancel",
      );
      if (result.notified) {
        for (const id of result.cancelled_ids ?? []) attentionOwners.delete(id);
      }
      return result;
    },

    async cancelTracked() {
      const owners = new Map();
      const broadSessions = new Set();
      for (const owner of attentionOwners.values()) {
        if (!owner.origin_session_id && !owner.run_id) continue;
        if (owner.origin_session_id && !owner.run_id) {
          broadSessions.add(owner.origin_session_id);
        }
      }
      for (const owner of attentionOwners.values()) {
        if (!owner.origin_session_id && !owner.run_id) continue;
        if (owner.run_id && broadSessions.has(owner.origin_session_id)) continue;
        const key = JSON.stringify(owner);
        owners.set(key, owner);
      }
      const results = await Promise.all(
        [...owners.values()].map((owner) => this.cancelOwned(owner)),
      );
      return {
        notified: results.every((result) => result.notified),
        operation: "cancel",
        cancelled_ids: results.flatMap((result) => result.cancelled_ids ?? []),
      };
    },
  };
}

export function humanCallToolResult(result) {
  if (result.notified) {
    const id = result.attention?.id;
    const summary = id
      ? `Human call sent. attention_id=${id}. Tell the human: ${result.request.message}`
      : result.operation === "clear"
        ? `Attention cleared for ${result.request.id}. pending_count=${result.pending_count}.`
        : `Attention ${result.operation} completed. pending_count=${result.pending_count}.`;
    return {
      structuredContent: result,
      content: [{ type: "text", text: `${summary}\n${JSON.stringify(result)}` }],
    };
  }

  const requested = result.request?.message
    ? ` Human action is still required: ${result.request.message}`
    : "";
  return {
    structuredContent: result,
    content: [
      {
        type: "text",
        text: `StateLamp notification FAILED: ${result.error}.${requested}\n${JSON.stringify(result)}`,
      },
    ],
  };
}
