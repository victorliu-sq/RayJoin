#!/usr/bin/env bash
set -euo pipefail

LBVH_STAMP="${DEPS_DIR}/.stamp-lbvh"

if [[ ! -f "${LBVH_STAMP}" ]]; then
  echo "[lbvh] Installing lbvh to ${DEPS_DIR} ..."
  pushd "${DEPS_DIR}" >/dev/null

  # download tmp project
  #  git clone "https://github.com/ToruNiina/lbvh.git"
  git clone "https://github.com/victorliu-sq/lbvh.git"
  pushd lbvh >/dev/null

  touch "${LBVH_STAMP}"
  echo "[lbvh] Installed."
else
  echo "[lbvh] Found stamp ${LBVH_STAMP}, skipping install."
fi
