export function resolveConfig(rawConfig) {
  const config = rawConfig && typeof rawConfig === "object" ? rawConfig : {};
  const agentId =
    typeof config.agentId === "string" && config.agentId.trim().length > 0
      ? config.agentId.trim()
      : undefined;
  const legacyAgentName =
    typeof config.agentName === "string" && config.agentName.trim().length > 0
      ? config.agentName.trim()
      : undefined;

  return {
    bridgeUrl:
      typeof config.bridgeUrl === "string"
        ? config.bridgeUrl
        : "http://127.0.0.1:18480/api/v1/status",
    timeoutMs:
      Number.isInteger(config.timeoutMs) && config.timeoutMs >= 100
        ? config.timeoutMs
        : 1500,
    attentionUrl:
      typeof config.attentionUrl === "string" ? config.attentionUrl : undefined,
    agentId: agentId ?? legacyAgentName ?? "openclaw",
    usedDeprecatedAgentName: agentId === undefined && legacyAgentName !== undefined,
  };
}
