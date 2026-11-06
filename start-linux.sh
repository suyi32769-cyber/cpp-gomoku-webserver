#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [ ! -x bin/webserver ]; then
  echo 'Build not found; run bash setup-linux.sh first.'
  exit 1
fi
mkdir -p logs
if [ -z "${DEEPSEEK_API_KEY:-}" ] && [ -f deepseek-key.local.txt ]; then
  IFS= read -r key_line < deepseek-key.local.txt || true
  key_line=${key_line#$'\xef\xbb\xbf'}
  key_line=${key_line%$'\r'}
  if [[ "$key_line" == DEEPSEEK_API_KEY=* ]]; then
    key_value=${key_line#DEEPSEEK_API_KEY=}
    if [ -n "$key_value" ] && [ "$key_value" != 'PASTE_YOUR_API_KEY_HERE' ]; then
      export DEEPSEEK_API_KEY="$key_value"
    fi
    unset key_value
  fi
  unset key_line
fi
if [ -z "${DEEPSEEK_API_KEY:-}" ]; then
  echo 'DeepSeek key not configured: edit deepseek-key.local.txt to enable AI.'
fi
echo 'Starting on http://127.0.0.1:8080/ . Press Ctrl+C to stop.'
exec ./bin/webserver
