export function createBridgePublisher({ bridgeUrl, timeoutMs, logger, fetchImpl = fetch }) {
  return async function publish(status) {
    try {
      const response = await fetchImpl(bridgeUrl, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(status),
        signal: AbortSignal.timeout(timeoutMs),
      });
      if (!response.ok) {
        logger.warn?.(`statelamp: Bridge returned HTTP ${response.status}`);
        return false;
      }
      return true;
    } catch (error) {
      logger.warn?.(`statelamp: Bridge publish failed (${String(error)})`);
      return false;
    }
  };
}
