import assert from "node:assert/strict";
import test from "node:test";

import {
  attentionForAgent,
  changedAgentStates,
  priorityAgent,
  selectedAgent,
} from "./static/console-state.js";

const agent = (name, state, message = state) => ({agent: name, state, message});

test("uses the complete Core2 priority order", () => {
  const ordered = ["human_required", "error", "waiting_approval", "working", "completed", "offline", "idle"];
  for (let index = 0; index < ordered.length; index += 1) {
    const agents = ordered.slice(index).reverse().map((state, n) => agent(String(n), state));
    assert.equal(priorityAgent(agents).state, ordered[index]);
  }
});

test("preserves response order as FIFO tie-breaker", () => {
  const agents = [agent("agent-b", "human_required"), agent("agent-a", "human_required")];
  assert.equal(priorityAgent(agents).agent, "agent-b");
});

test("automatic selection follows priority while pinning remains stable", () => {
  const agents = [agent("agent-a", "working"), agent("agent-b", "waiting_approval")];
  assert.equal(selectedAgent(agents).agent, "agent-b");
  assert.equal(selectedAgent(agents, "agent-a").agent, "agent-a");
  assert.equal(selectedAgent(agents, "missing").agent, "agent-b");
});

test("filters attention to the selected logical agent", () => {
  const items = [{agent: "agent-a", id: "1"}, {agent: "agent-b", id: "2"}];
  assert.deepEqual(attentionForAgent(items, "agent-b"), [items[1]]);
});

test("deduplicates events independently per agent", () => {
  const initial = [agent("agent-a", "idle"), agent("agent-b", "idle")];
  const first = changedAgentStates(new Map(), initial);
  assert.deepEqual(first.changed, initial);
  const same = changedAgentStates(first.next, initial);
  assert.deepEqual(same.changed, []);
  const updated = changedAgentStates(same.next, [agent("agent-a", "working"), initial[1]]);
  assert.deepEqual(updated.changed.map(item => item.agent), ["agent-a"]);
});

test("supports old-Bridge fallback represented as one agent", () => {
  const legacy = [agent("openclaw", "working")];
  assert.equal(selectedAgent(legacy).agent, "openclaw");
});
