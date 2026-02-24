#!/usr/bin/env bash
set -euo pipefail

SLANG_VER="2026.3.1"
SLANG_STAMP="${DEPS_DIR}/.stamp-slang"

if [[ ! -f "${SLANG_STAMP}" ]]; then
  echo "[slang] Installing Slang v${SLANG_VER} to ${DEPS_DIR} ..."
  mkdir -p "${DEPS_DIR}" "${DEPS_TMP_DIR}"
  pushd "${DEPS_TMP_DIR}" >/dev/null

  TARBALL="slang-${SLANG_VER}-linux-x86_64.tar.gz"
  URL="https://github.com/shader-slang/slang/releases/download/v${SLANG_VER}/slang-${SLANG_VER}-linux-x86_64.tar.gz"
  download "${URL}" "${TARBALL}"

  tar -xzf "${TARBALL}" -C "${DEPS_TMP_DIR}"

  cp -a "${DEPS_TMP_DIR}" "${DEPS_DIR}/slang"

  touch "${SLANG_STAMP}"
  echo "[slang] Installed."
else
  echo "[slang] Found stamp ${SLANG_STAMP}, skipping install."
fi