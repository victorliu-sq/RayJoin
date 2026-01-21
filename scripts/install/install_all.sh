#!/usr/bin/env bash
set -euo pipefail

# --- Installation Config
export DEPS_DIR="${PROJECT_DIR}/deps"
export DEPS_TMP_DIR="${DEPS_DIR}/tmp"
export INSTALLER_DIR="${SCRIPTS_DIR}/install"

# --- Create Directories ----------------------------------------------------
mkdir -p \
  ${DEPS_DIR} \
  ${DEPS_TMP_DIR}

# --- Install optix ----------------------------------------------------
# bash + path (single argument) + subsequent arguments
bash "${INSTALLER_DIR}/installer_optix.sh"

# --- Install gflags  ----------------------------------------------------
bash "${INSTALLER_DIR}/installer_gflags.sh"

# --- Install glog  ----------------------------------------------------
bash "${INSTALLER_DIR}/installer_glog.sh"

# --- Install gtest  ----------------------------------------------------
#bash "${INSTALLER_DIR}/installer_gtest.sh"

# --- Install google benchmark  ----------------------------------------------------
#bash "${INSTALLER_DIR}/installer_gbenchmark.sh"

#bash "${INSTALLER_DIR}/installer_python.sh"

# --- Install Ligra ----------------------------------------------------
#bash "${SCRIPTS_DIR}/install/install_ligra_openmp.sh" # Install Ligra

# --- Install Gunrock ----------------------------------------------------
#bash "${SCRIPTS_DIR}/install/install_gunrock.sh" # Install Gunrock

#export LD_LIBRARY_PATH="${DEPS_DIR}/lib:${PROJECT_DIR}/bin:${LD_LIBRARY_PATH:-}"
#echo "[INFO] LD_LIBRARY_PATH set to: $LD_LIBRARY_PATH"

# remove the tmp directory
rm -rf ${DEPS_TMP_DIR}