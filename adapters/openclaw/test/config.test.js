import assert from "node:assert/strict";
import test from "node:test";

import { resolveConfig } from "../config.js";

test("defaults logical identity to openclaw for v1.2 compatibility", () => {
  assert.equal(resolveConfig().agentId, "openclaw");
  assert.equal(resolveConfig({ agentId: "   " }).agentId, "openclaw");
});

test("uses and trims canonical agentId", () => {
  const config = resolveConfig({ agentId: "  agent-a  " });

  assert.equal(config.agentId, "agent-a");
  assert.equal(config.usedDeprecatedAgentName, false);
});

test("accepts non-empty agentName as a deprecated migration alias", () => {
  const config = resolveConfig({ agentId: " ", agentName: " agent-b " });

  assert.equal(config.agentId, "agent-b");
  assert.equal(config.usedDeprecatedAgentName, true);
});

test("agentId takes precedence over agentName", () => {
  const config = resolveConfig({ agentId: "agent-a", agentName: "agent-b" });

  assert.equal(config.agentId, "agent-a");
  assert.equal(config.usedDeprecatedAgentName, false);
});

test("rejects whitespace-only legacy identity", () => {
  const config = resolveConfig({ agentName: "\t \n" });

  assert.equal(config.agentId, "openclaw");
  assert.equal(config.usedDeprecatedAgentName, false);
});
