#!/usr/bin/env bash
set -euo pipefail

GBM_VER="1.9.4"
GBM_STAMP="${DEPS_DIR}/.stamp-benchmark"

if [[ ! -f "${GBM_STAMP}" ]]; then
  echo "[google-benchmark] Installing benchmark v${GBM_STAMP} to ${DEPS_DIR} ..."
  pushd "${DEPS_TMP_DIR}" >/dev/null

  TARBALL="benchmark-${GBM_VER}.tar.gz"
  URL="https://github.com/google/benchmark/archive/refs/tags/v${GBM_VER}.tar.gz"
  download "$URL" "$TARBALL"

  rm -rf "benchmark-${GBM_VER}"
  tar zxf "$TARBALL"
  pushd "benchmark-${GBM_VER}" >/dev/null

    rm -rf build
    mkdir -p build && cd build

    cmake \
      -DBUILD_SHARED_LIBS=ON \
      -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on \
      -DCMAKE_INSTALL_PREFIX="${DEPS_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      ..

    cmake --build . --target install -j

  popd >/dev/null # benchmark-${GBM-VER}
  popd >/dev/null # tmp

  # remove tmp files
  rm "${DEPS_TMP_DIR}/${TARBALL}"
  rm -rf "${DEPS_TMP_DIR}/benchmark-${GBM_VER}"

  touch "${GBM_STAMP}"
  echo "[google-benchmark] Installed."
else
  echo "[google-benchmark] Found stamp ${GBM_STAMP}, skipping install."
fi
