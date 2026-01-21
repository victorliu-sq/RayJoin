#!/usr/bin/env bash
set -euo pipefail


GTEST_VER="1.17.0"
GTEST_STAMP="${DEPS_DIR}/.stamp-googletest"

if [[ ! -f "${GTEST_STAMP}" ]]; then
  echo "[google-test] Installing googletest v${GTEST_STAMP} to ${DEPS_DIR} ..."
  pushd "${DEPS_TMP_DIR}" >/dev/null

  TARBALL="googletest-${GTEST_VER}.tar.gz"
  URL="https://github.com/google/googletest/releases/download/v${GTEST_VER}/googletest-${GTEST_VER}.tar.gz"
  download "$URL" "$TARBALL"

  rm -rf "googletest-${GTEST_VER}"
  tar -xzf "$TARBALL"
  pushd "googletest-${GTEST_VER}" >/dev/null

  rm -rf build
  mkdir -p build && cd build

  cmake \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_GTEST=OFF \
    -DCMAKE_INSTALL_PREFIX="${DEPS_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    ..

  cmake --build . --target install -j

  popd >/dev/null # googletest-${GBM-VER}
  popd >/dev/null # tmp

  # remove tmp files
  rm "${DEPS_TMP_DIR}/${TARBALL}"
  rm -rf "${DEPS_TMP_DIR}/googletest-${GTEST_VER}"

  touch "${GTEST_STAMP}"
  echo "[google-test] Installed."

  # ---------------------------------------------------------
  # --- Patch nvcc compatibility issue (Ref -> RefMethod) ---
    GTEST_HEADER="${DEPS_DIR}/include/gtest/gtest-matchers.h"
    if [[ -f "${GTEST_HEADER}" ]]; then
      echo "[google-test] Patching nvcc incompatibility in gtest-matchers.h ..."
      # Create a backup before modifying
      cp "${GTEST_HEADER}" "${GTEST_HEADER}.bak"

      # Replace all occurrences of 'Ref' with 'RefMethod' (but skip std::ref)
      sed -i -E 's/([^[:alnum:]_])Ref([^[:alnum:]_])/\1RefMethod\2/g' "${GTEST_HEADER}"

      echo "[google-test] Patch applied successfully to ${GTEST_HEADER}."
    else
      echo "[google-test] WARNING: gtest-matchers.h not found at ${GTEST_HEADER}"
    fi
else
  echo "[google-test] Found stamp ${GTEST_STAMP}, skipping install."
fi
