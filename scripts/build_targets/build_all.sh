#!/usr/bin/env bash
set -euo pipefail

echo "[BUILD] Configure & Build (Release)"

BUILD_DIR="$PROJECT_DIR/build"
OUTPUT_BIN_DIR="$BUILD_DIR/bin"

# Build All Targets
TARGETS=(
# Optix
polyover_exec
# Vk
polyover_vk_exec_ns
)

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="${OUTPUT_BIN_DIR}"

cmake --build build \
  --target "${TARGETS[@]}" -j

echo "[BUILD] Build Complete"
