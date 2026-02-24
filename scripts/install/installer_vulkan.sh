#!/usr/bin/env bash
set -euo pipefail

VULKAN_VER="1.4.341.1"
VULKAN_STAMP="${DEPS_DIR}/.stamp-vulkansdk"

if [[ ! -f "${VULKAN_STAMP}" ]]; then
  echo "[vulkansdk] Installing Vulkan SDK v${VULKAN_VER} to ${DEPS_DIR} ..."
  mkdir -p "${DEPS_DIR}" "${DEPS_TMP_DIR}"
  pushd "${DEPS_TMP_DIR}" >/dev/null

  TARBALL="vulkansdk-linux-x86_64-${VULKAN_VER}.tar.xz"
  URL="https://sdk.lunarg.com/sdk/download/${VULKAN_VER}/linux/${TARBALL}"
  download "${URL}" "${TARBALL}"

  tar -xJf "${TARBALL}" -C "${DEPS_TMP_DIR}"

  cp -a "${DEPS_TMP_DIR}/${VULKAN_VER}" "${DEPS_DIR}/vulkansdk"

  touch "${VULKAN_STAMP}"
  echo "[vulkansdk] Installed."
else
  echo "[vulkansdk] Found stamp ${VULKAN_STAMP}, skipping install."
fi