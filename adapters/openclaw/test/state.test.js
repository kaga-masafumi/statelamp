import assert from "node:assert/strict";
import test from "node:test";

import { RunStateCoordinator } from "../state.js";

test("maps a successful run to working then completed", () => {
  const coordinator = new RunStateCoordinator();

  assert.equal(coordinator.start("run-1").state, "working");
  assert.equal(coordinator.end("run-1", true).state, "completed");
});

test("uses the configured logical agent identity", () => {
  const coordinator = new RunStateCoordinator("agent-a");

  assert.deepEqual(coordinator.ready(), {
    agent: "agent-a",
    state: "idle",
    message: "agent-a ready",
  });
  assert.equal(coordinator.start("run-1").agent, "agent-a");
  assert.equal(coordinator.requestApproval("exec-1").agent, "agent-a");
});

test("maps a failed run to error", () => {
  const coordinator = new RunStateCoordinator();

  coordinator.start("run-1");

  assert.equal(coordinator.end("run-1", false, "failure").state, "error");
});

test("stays working while another run remains active", () => {
  const coordinator = new RunStateCoordinator();
  coordinator.start("run-1");
  coordinator.start("run-2");

  assert.equal(coordinator.end("run-1", true).state, "working");
  assert.equal(coordinator.end("run-2", true).state, "completed");
});

test("approval overlays run state and returns to working when resolved", () => {
  const coordinator = new RunStateCoordinator();
  coordinator.start("run-1");

  assert.equal(coordinator.requestApproval("exec-1").state, "waiting_approval");
  assert.equal(coordinator.resolveApproval("exec-1").state, "working");
});

test("stays waiting until all exec and plugin approvals resolve", () => {
  const coordinator = new RunStateCoordinator();
  coordinator.requestApproval("exec-1");
  coordinator.requestApproval("plugin:1");

  assert.equal(coordinator.resolveApproval("exec-1").state, "waiting_approval");
  assert.equal(coordinator.resolveApproval("plugin:1").state, "idle");
});

test("restores terminal state when an approval outlives a run", () => {
  const coordinator = new RunStateCoordinator();
  coordinator.start("run-1");
  coordinator.requestApproval("exec-1");
  coordinator.end("run-1", false, "failure");

  assert.equal(coordinator.current().state, "waiting_approval");
  assert.equal(coordinator.resolveApproval("exec-1").state, "error");
});

test("reconnect backfill replaces stale approval IDs", () => {
  const coordinator = new RunStateCoordinator();
  coordinator.requestApproval("stale");

  assert.equal(
    coordinator.replaceApprovals(["exec-1", "plugin:1", "exec-1"]).state,
    "waiting_approval",
  );
  assert.equal(coordinator.resolveApproval("exec-1").state, "waiting_approval");
  assert.equal(coordinator.resolveApproval("plugin:1").state, "idle");
});

test("exposes an owner run only when it is unambiguous", () => {
  const coordinator = new RunStateCoordinator();
  assert.equal(coordinator.activeRunId(), undefined);
  coordinator.start("run-1");
  assert.equal(coordinator.activeRunId(), "run-1");
  coordinator.start("run-2");
  assert.equal(coordinator.activeRunId(), undefined);
  coordinator.end("run-1", true);
  assert.equal(coordinator.activeRunId(), "run-2");
});
