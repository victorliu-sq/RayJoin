#!/usr/bin/env bash
set -euo pipefail

PY_REQ="$SCRIPTS_DIR/install/requirements.txt"
ENV_NAME="xb-env"

echo "[INSTALL] Install Python Environment ..."
#if [[ ! $(conda info --env | grep xb-env) ]]; then
##  conda remove -n xb-env --all -y
#
#  conda create -n xb-env python=3.11 -y
#
##  conda run -n xb-env --no-capture-output python3 -m pip install --upgrade pip
##  conda run -n xb-env --no-capture-output python3 -m pip install -r $PY_REQ
#  conda install -n xb-env -c conda-forge --file $PY_REQ -y
#
#  echo "[INSTALL] Install Python Environment Done!"
#else
#  echo "[INSTALL] Install Python Environment Already Exists!"
#fi


# ------------------------------------------------------------
# Create env if it does not exist
# ------------------------------------------------------------
if ! conda info --env | awk '{print $1}' | grep -qx "$ENV_NAME"; then
  echo "[INSTALL] Creating conda env $ENV_NAME"
  conda create -n "$ENV_NAME" python=3.11 -y
else
  echo "[INSTALL] Conda env $ENV_NAME already exists"
fi

# ------------------------------------------------------------
# ALWAYS ensure required packages are installed
# (installs only missing ones)
# ------------------------------------------------------------
echo "[INSTALL] Syncing Python packages from requirements.txt"
#conda install -n "$ENV_NAME" -c conda-forge --file "$PY_REQ" -y
conda run -n "$ENV_NAME" python -m pip install -r "$PY_REQ"

echo "[INSTALL] Python Environment Ready!"