#!/usr/bin/env bash
set -euo pipefail

OPTIX_DIR="${DEPS_DIR}/optix"
OPTIX_STAMP="${DEPS_DIR}/.stamp-optix"
OPTIX_INSTALLER="${INSTALLER_DIR}/NVIDIA-OptiX-SDK-8.0.0-linux64-x86_64.sh"

if [[ ! -f "${OPTIX_STAMP}" ]]; then
  echo "[optix] Installing OptiX to ${OPTIX_DIR} ..."

  bash "${OPTIX_INSTALLER}" \
    --prefix="${OPTIX_DIR}" \
    --exclude-subdir \
    --skip-license

  touch "${OPTIX_STAMP}"
  echo "[optix] Installed."
else
  echo "[optix] Found stamp ${OPTIX_STAMP}, skipping install."
fi
