#!/usr/bin/env bash
set -euo pipefail

echo "[BUILD] Configure & Build (Release)"

parse_build_mode_args "[BUILD]" "$@"

BUILD_DIR="$PROJECT_DIR/build"
OUTPUT_BIN_DIR="$BUILD_DIR/bin"

RAYJOIN_BUILD_OPTIX="ON"
RAYJOIN_BUILD_VULKAN="ON"

# Build All Targets
OPTIX_TARGETS=(
# Optix
polyover_exec
polyover_exec_native
)

VULKAN_TARGETS=(
# Vk
#polyover_vk_exec
polyover_vk_exec_ns
)

TARGETS=()

case "${BUILD_MODE}" in
  optix)
    RAYJOIN_BUILD_OPTIX="ON"
    RAYJOIN_BUILD_VULKAN="OFF"
    TARGETS=("${OPTIX_TARGETS[@]}")
    ;;
  vk)
    RAYJOIN_BUILD_OPTIX="OFF"
    RAYJOIN_BUILD_VULKAN="ON"
    TARGETS=("${VULKAN_TARGETS[@]}")
    ;;
  all)
    RAYJOIN_BUILD_OPTIX="ON"
    RAYJOIN_BUILD_VULKAN="ON"
    TARGETS=("${OPTIX_TARGETS[@]}" "${VULKAN_TARGETS[@]}")
    ;;
esac

echo "[BUILD] BUILD_MODE=${BUILD_MODE}"
echo "[BUILD] RAYJOIN_BUILD_OPTIX=${RAYJOIN_BUILD_OPTIX}"
echo "[BUILD] RAYJOIN_BUILD_VULKAN=${RAYJOIN_BUILD_VULKAN}"
echo "[BUILD] TARGETS=${TARGETS[*]}"

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="${OUTPUT_BIN_DIR}" \
  -DRAYJOIN_BUILD_OPTIX="${RAYJOIN_BUILD_OPTIX}" \
  -DRAYJOIN_BUILD_VULKAN="${RAYJOIN_BUILD_VULKAN}"

cmake --build build \
  --target "${TARGETS[@]}" -j

echo "[BUILD] Build Complete"