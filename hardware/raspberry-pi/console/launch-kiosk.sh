#!/bin/sh
set -eu

console_url="${STATELAMP_CONSOLE_URL:-http://127.0.0.1:18880}"

exec cage -s -- chromium \
  --kiosk \
  --no-first-run \
  --disable-session-crashed-bubble \
  --disable-infobars \
  --disable-pinch \
  --overscroll-history-navigation=0 \
  --password-store=basic \
  "$console_url"
