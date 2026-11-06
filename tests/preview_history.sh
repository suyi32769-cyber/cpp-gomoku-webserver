#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
testdir=$(mktemp -d /tmp/gomoku-ui-XXXXXX)
export GOMOKU_DB_PATH="$testdir/test.sqlite3"
unset DEEPSEEK_API_KEY
exec ./bin/webserver
