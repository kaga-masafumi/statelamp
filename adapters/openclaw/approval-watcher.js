const REQUESTED_EVENTS = new Set([
  "exec.approval.requested",
  "plugin.approval.requested",
]);

const RESOLVED_EVENTS = new Set([
  "exec.approval.resolved",
  "plugin.approval.resolved",
]);

export class ApprovalWatcher {
  #client = null;
  #coordinator;
  #createClient;
  #eventBuffer = [];
  #logger;
  #publish;
  #startClient;
  #syncing = false;
  #syncQueue = Promise.resolve();

  constructor({ coordinator, createClient, startClient, publish, logger }) {
    this.#coordinator = coordinator;
    this.#createClient = createClient;
    this.#startClient = startClient;
    this.#publish = publish;
    this.#logger = logger;
  }

  async start() {
    try {
      this.#client = await this.#createClient({
        onEvent: (event) => this.#handleEvent(event),
        onHelloOk: () => this.#scheduleSync(),
        onConnectError: (error) =>
          this.#logger.warn?.(`statelamp: approval client connect failed (${error})`),
        onReconnectPaused: (info) =>
          this.#logger.warn?.(
            `statelamp: approval client reconnect paused (${JSON.stringify(info)})`,
          ),
        onClose: (code, reason) =>
          this.#logger.warn?.(`statelamp: approval client closed (${code}: ${reason})`),
      });
      const readiness = await this.#startClient(this.#client);
      if (!readiness?.ready) {
        this.#logger.warn?.("statelamp: approval client did not become ready");
      }
    } catch (error) {
      // Approval observation is optional: never prevent Gateway startup.
      this.#logger.warn?.(`statelamp: approval watcher start failed (${error})`);
    }
  }

  async stop() {
    const client = this.#client;
    this.#client = null;
    if (!client) return;
    await client.stopAndWait?.().catch(() => client.stop?.());
  }

  #scheduleSync() {
    this.#syncQueue = this.#syncQueue
      .then(() => this.#backfill())
      .catch((error) =>
        this.#logger.warn?.(`statelamp: approval backfill failed (${error})`),
      );
  }

  async #backfill() {
    const client = this.#client;
    if (!client) return;

    this.#syncing = true;
    this.#eventBuffer = [];
    try {
      const [execPending, pluginPending] = await Promise.all([
        client.request("exec.approval.list", {}),
        client.request("plugin.approval.list", {}),
      ]);
      this.#coordinator.replaceApprovals([
        ...execPending.map((request) => request.id),
        ...pluginPending.map((request) => request.id),
      ]);
    } catch (error) {
      this.#logger.warn?.(`statelamp: approval backfill failed (${error})`);
    }

    const buffered = this.#eventBuffer;
    this.#eventBuffer = [];
    for (const event of buffered) this.#applyEvent(event);
    this.#syncing = false;
    await this.#emitCurrent();
  }

  #handleEvent(event) {
    if (!REQUESTED_EVENTS.has(event.event) && !RESOLVED_EVENTS.has(event.event)) return;
    if (this.#syncing) {
      this.#eventBuffer.push(event);
      return;
    }
    this.#applyEvent(event);
    void this.#emitCurrent();
  }

  #applyEvent(event) {
    const id = event.payload?.id;
    if (REQUESTED_EVENTS.has(event.event)) this.#coordinator.requestApproval(id);
    else if (RESOLVED_EVENTS.has(event.event)) this.#coordinator.resolveApproval(id);
  }

  async #emitCurrent() {
    try {
      await this.#publish(this.#coordinator.current());
    } catch (error) {
      // Keep a second safety boundary even if a custom publisher is injected.
      this.#logger.warn?.(`statelamp: approval status publish failed (${error})`);
    }
  }
}
