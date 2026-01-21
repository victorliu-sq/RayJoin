#!/usr/bin/env bash
set -euo pipefail

#GFLAGS_VER="2.2.2"
GFLAGS_STAMP="${DEPS_DIR}/.stamp-gflags"

if [[ ! -f "${GFLAGS_STAMP}" ]]; then
  echo "[gflags] Installing gflags to ${DEPS_DIR} ..."
  pushd $DEPS_TMP_DIR >/dev/null

  # tmp projects
  git clone https://github.com/gflags/gflags.git

  pushd "gflags" >/dev/null

  # --- install gflags following its own step
  rm -rf build
  mkdir -p build && cd build

  cmake -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_INSTALL_PREFIX="${DEPS_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        ..
  cmake --build . --target install -j

  popd >/dev/null # gflags-$GFLAGS_VER
  popd >/dev/null # tmp

  # remove tmp files
  rm -rf "${DEPS_TMP_DIR}/gflags"

  touch "${GFLAGS_STAMP}"
  echo "[gflags] Installed."
else
  echo "[gflags] Found stamp ${GFLAGS_STAMP}, skipping install."
fi
