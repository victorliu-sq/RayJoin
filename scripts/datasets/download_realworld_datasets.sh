#!/usr/bin/env bash
set -euo pipefail

download_and_extract_dataset() {
  local DATASET_NAME="$1"
  local URL="$2"
  local DATASET_STAMP=".stamp.${DATASET_NAME}.Dataset"
  local TARBALL="${DATASET_NAME}.tar.gz"
  local PY_SCRIPT="${SCRIPTS_DIR}/datasets/download_dataset.py"

  if [[ ! -f "${DATASET_STAMP}" ]]; then
    echo "[download] Downloading ${DATASET_NAME} ..."
    # Download tarball
    # wget -O "${TARBALL}" "${URL}"
    conda run -n xb-env --live-stream python3 "${PY_SCRIPT}" "${URL}" "${TARBALL}"

    # Extract
    tar -xzf "${TARBALL}"
    # Clean up tarball
    rm -f "${TARBALL}"
    # Mark as downloaded
    touch "${DATASET_STAMP}"
    echo "[download] ${DATASET_NAME} done!"
  else
    echo "[download] ${DATASET_NAME} already exists, skipping download"
  fi
}

echo "[Datasets] Download Realworld Datasets"

DATASETS_DIR="${PROJECT_DIR}/data/realworld"
mkdir -p ${DATASETS_DIR}

pushd ${DATASETS_DIR} >/dev/null

download_and_extract_dataset "Aquifers" "https://buckeyemailosu-my.sharepoint.com/:u:/g/personal/liu_11080_buckeyemail_osu_edu/IQAcz1zSV2bfSZDQGAMsB6MDAfKmewJcJOi6OtntHsbiIIw?e=b8bu3v"
download_and_extract_dataset "dtl_cnty" "https://buckeyemailosu-my.sharepoint.com/:u:/g/personal/liu_11080_buckeyemail_osu_edu/IQCnae4r4y3FS77FpkLIuS_IASwrChIS9036yhpDbCF96Ww?e=tvcJGG"
download_and_extract_dataset "Parks" "https://buckeyemailosu-my.sharepoint.com/:u:/g/personal/liu_11080_buckeyemail_osu_edu/IQDLn5SUeYYST4yMWdxqzJq9AWxUkalj2hWjzlOifNn8-fw?e=7SSg5H"
download_and_extract_dataset "USACensusBlockGroupBoundaries" "https://buckeyemailosu-my.sharepoint.com/:u:/g/personal/liu_11080_buckeyemail_osu_edu/IQC0g2oX2GvlQInAHZ20H2YaAcqkTwtqzPPwOXrdwbiu1sg?e=XdTDlI"
download_and_extract_dataset "USADetailedWaterBodies" "https://buckeyemailosu-my.sharepoint.com/:u:/g/personal/liu_11080_buckeyemail_osu_edu/IQADQApCfNGdQKyUpWVvu9xYAYW8-1wfy1AmU3ns-xbu0Rw?e=QoTmG7"
download_and_extract_dataset "USAZIPCodeArea" "https://buckeyemailosu-my.sharepoint.com/:u:/g/personal/liu_11080_buckeyemail_osu_edu/IQBR8-P93KjPR7e14GtFkR5OAbSW_X9w-KrGlNnduR0axLI?e=vZceQ3"

popd >/dev/null