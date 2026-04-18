#!/usr/bin/env bash
set -euo pipefail

# Compared to $PWD, this command improves the portability of scripts
# Not where you ran this script,PROJECT_DIR and SCRIPTS_DIRS will be evaluated based on the absoluate paths of scripts.
# two parts: (1) cd a directory  (2) pwd
export PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export SCRIPTS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
export DEPS_DIR="${PROJECT_DIR}/deps"

# Shared Conda environment name
export CONDA_ENV_NAME="${CONDA_ENV_NAME:-rayjoin-env}"

# downloader
download() {
  local url="$1" out="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -L "$url" -o "$out"
  elif command -v wget >/dev/null 2>&1; then
    wget "$url" -O "$out"
  else
    echo "ERROR: need curl or wget" >&2
    exit 1
  fi
}
export -f download

# Shared build mode parser
# Usage:
#   BUILD_MODE="all"
#   parse_build_mode_args "[RUNME]" "$@"
# or
#   BUILD_MODE="all"
#   parse_build_mode_args "[BUILD]" "$@"
parse_build_mode_args() {
  local prefix="$1"
  shift

  BUILD_MODE="all"

  for arg in "$@"; do
    case "$arg" in
      --optix)
        BUILD_MODE="optix"
        ;;
      --vk)
        BUILD_MODE="vk"
        ;;
      --all)
        BUILD_MODE="all"
        ;;
      *)
        echo "${prefix} Unknown argument: $arg" >&2
        echo "Usage: $0 [--optix | --vk | --all]" >&2
        exit 1
        ;;
    esac
  done
}
export -f parse_build_mode_args