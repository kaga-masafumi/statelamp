export class RunStateCoordinator {
  #agent;
  #activeRuns = new Set();
  #pendingApprovals = new Set();
  #normalStatus;

  constructor(agent = "openclaw") {
    this.#agent = agent;
    this.#normalStatus = this.#status("idle", `${agent} ready`);
  }

  #status(state, message) {
    return { agent: this.#agent, state, message };
  }

  ready() {
    this.#activeRuns.clear();
    this.#normalStatus = this.#status("idle", `${this.#agent} ready`);
    return this.current();
  }

  start(runId) {
    if (runId) this.#activeRuns.add(runId);
    this.#normalStatus = this.#status("working", `${this.#agent} is working`);
    return this.current();
  }

  end(runId, success, error) {
    if (runId) this.#activeRuns.delete(runId);
    if (this.#activeRuns.size > 0) {
      this.#normalStatus = this.#status(
        "working",
        `${this.#activeRuns.size} ${this.#agent} run(s) active`,
      );
      return this.current();
    }
    this.#normalStatus = success
      ? this.#status("completed", `${this.#agent} run completed`)
      : this.#status(
          "error",
          error
            ? `${this.#agent} run failed`
            : `${this.#agent} run ended unsuccessfully`,
        );
    return this.current();
  }

  requestApproval(id) {
    if (id) this.#pendingApprovals.add(id);
    return this.current();
  }

  resolveApproval(id) {
    if (id) this.#pendingApprovals.delete(id);
    return this.current();
  }

  replaceApprovals(ids) {
    this.#pendingApprovals = new Set(ids.filter(Boolean));
    return this.current();
  }

  current() {
    if (this.#pendingApprovals.size > 0) {
      return this.#status(
        "waiting_approval",
        `${this.#pendingApprovals.size} approval(s) pending`,
      );
    }
    return { ...this.#normalStatus };
  }

  // A run ID is safe to attach only when this coordinator has one unambiguous
  // active run. Callers must fall back to the session owner when there are
  // concurrent runs, rather than guessing and cancelling the wrong request.
  activeRunId() {
    return this.#activeRuns.size === 1 ? this.#activeRuns.values().next().value : undefined;
  }
}
