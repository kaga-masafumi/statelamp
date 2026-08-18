# OpenClaw integration

日本語版: [README.ja.md](README.ja.md)

This directory contains the StateLamp OpenClaw plugin. It is the project's reference integration. It publishes lifecycle state, watches operator approval events, and exposes `human_required` and `human_required_clear` tools.

A human call returns an exact attention ID. The agent should retain that ID and clear only it after the requested action is complete. If the Bridge cannot be reached, the result reports the failure while preserving `human_action_still_required: true`.

Configure the plugin with a Bridge URL reachable by OpenClaw. Keep credentials and machine-specific configuration outside this repository.

Run tests with `npm install && npm test`.
