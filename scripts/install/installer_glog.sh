#!/usr/bin/env bash
set -euo pipefail

GLOG_STAMP="${DEPS_DIR}/.stamp-glog"

if [[ ! -f "${GLOG_STAMP}" ]]; then
  echo "[glog] Installing glog to ${DEPS_DIR} ..."
  pushd "${DEPS_TMP_DIR}" >/dev/null

  # download tmp project
  git clone "https://github.com/google/glog.git"
  pushd glog >/dev/null

  # install glog
  rm -rf build
  mkdir -p build && cd build

  cmake \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_GTEST=OFF \
    -DCMAKE_INSTALL_PREFIX="${DEPS_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${DEPS_DIR}" \
    ..

  cmake --build . --target install -j

  popd >/dev/null # glog-$GLOG_VER
  popd >/dev/null # tmp

  # remove tmp project
  rm -rf "$DEPS_TMP_DIR/glog"

  touch "${GLOG_STAMP}"
  echo "[glog] Installed."
else
  echo "[glog] Found stamp ${GLOG_STAMP}, skipping install."
fi
