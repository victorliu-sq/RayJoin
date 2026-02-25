#!/usr/bin/env bash
set -euo pipefail

VMA_STAMP="${DEPS_DIR}/.stamp-vma"

if [[ ! -f "${VMA_STAMP}" ]]; then
  echo "[vma] Installing vma to ${DEPS_DIR} ..."
  pushd "${DEPS_TMP_DIR}" >/dev/null

  # download tmp project
  git clone "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git" vma
  pushd vma >/dev/null

  # install vma
  rm -rf build
  cmake -S . -B build

  # install (per docs) into deps/
  cmake --install build --prefix "${DEPS_DIR}"

  popd >/dev/null # vma
  popd >/dev/null # tmp

  # remove tmp project
  rm -rf "$DEPS_TMP_DIR/vma"

  touch "${VMA_STAMP}"
  echo "[vma] Installed."
else
  echo "[vma] Found stamp ${VMA_STAMP}, skipping install."
fi
