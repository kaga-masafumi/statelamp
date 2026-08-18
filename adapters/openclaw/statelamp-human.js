#!/usr/bin/env node

import { createHumanCallClient, resolveAttentionUrl } from "./human-call.js";

function usage() {
  return `Usage:
  statelamp-human --reason REASON --message MESSAGE [--agent NAME]
  statelamp-human --clear ATTENTION_ID

Environment:
  STATELAMP_URL       status URL (default http://127.0.0.1:18480/api/v1/status)
  STATELAMP_TIMEOUT_MS  request timeout (default 1500)`;
}

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 2) {
    const key = argv[index];
    const value = argv[index + 1];
    if (!key?.startsWith("--") || value === undefined) throw new Error(usage());
    values[key.slice(2)] = value;
  }
  return values;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const bridgeUrl = process.env.STATELAMP_URL ?? "http://127.0.0.1:18480/api/v1/status";
  const timeoutMs = Number.parseInt(process.env.STATELAMP_TIMEOUT_MS ?? "1500", 10);
  const client = createHumanCallClient({
    attentionUrl: resolveAttentionUrl(bridgeUrl),
    timeoutMs: Number.isInteger(timeoutMs) && timeoutMs > 0 ? timeoutMs : 1500,
    logger: console,
  });

  let result;
  if (args.clear) {
    result = await client.clear(args.clear);
  } else {
    if (!args.reason || !args.message) throw new Error(usage());
    result = await client.request({
      agent: args.agent ?? "openclaw",
      reason: args.reason,
      message: args.message,
    });
  }
  console.log(JSON.stringify(result, null, 2));
  process.exitCode = result.notified ? 0 : 1;
}

main().catch((error) => {
  console.error(String(error));
  process.exitCode = 2;
});
