#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
for cmd in cmake g++ make; do
  if ! command -v "$cmd" >/dev/null; then
    echo "Missing $cmd. On Ubuntu run: sudo apt update && sudo apt install -y build-essential cmake"
    exit 1
  fi
done
mkdir -p logs
cmake -S . -B build
cmake --build build --parallel 2
echo 'Build complete. Run ./start-linux.sh'
