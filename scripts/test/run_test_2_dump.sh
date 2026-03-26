#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"

# =========================================================
# Run Vulkan with dumps enabled
./build/bin/polyover_vk_exec_ns \
  -poly1 data/realworld_small/br_county_clean_25_odyssey_final.txt \
  -poly2 data/realworld_small/br_soil_ascii_odyssey_final.txt \
  -output  "${base_dir}/results/br_countyXbr_soil_result_vk.txt" \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=pip,lsi,output \
  -dump_dir="${base_dir}"

# =========================================================
# Run OptiX with dumps enabled
./build/bin/polyover_exec \
  -poly1 data/realworld_small/br_county_clean_25_odyssey_final.txt \
  -poly2 data/realworld_small/br_soil_ascii_odyssey_final.txt \
  -output  "${base_dir}/results/br_countyXbr_soil_result_optix.txt" \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=pip,lsi,output \
  -dump_dir="${base_dir}"

status=0
cmp_script="./scripts/test/compare_csv.sh"

run_compare() {
  local label="$1"
  local f1="$2"
  local f2="$3"

  echo "========================================"
  echo "Test: $label"
  echo "  file 1: $f1"
  echo "  file 2: $f2"
  echo "========================================"

  if [[ ! -x "$cmp_script" ]]; then
    echo "Result: FAIL"
    echo "Reason: compare script missing or not executable: $cmp_script"
    status=1
    echo
    return
  fi

  if [[ ! -f "$f1" ]]; then
    echo "Result: FAIL"
    echo "Reason: missing file: $f1"
    status=1
    echo
    return
  fi

  if [[ ! -f "$f2" ]]; then
    echo "Result: FAIL"
    echo "Reason: missing file: $f2"
    status=1
    echo
    return
  fi

  if "$cmp_script" "$f1" "$f2"; then
    echo "Result: PASS"
  else
    echo "Result: FAIL"
    status=1
  fi

  echo
}

# -------------------------
# PIP comparisons
# -------------------------
run_compare "PIP map 0" \
  "${base_dir}/results_pip/optix_pip_map_0.csv" \
  "${base_dir}/results_pip/vulkan_pip_map_0.csv"

run_compare "PIP map 1" \
  "${base_dir}/results_pip/optix_pip_map_1.csv" \
  "${base_dir}/results_pip/vulkan_pip_map_1.csv"

# -------------------------
# LSI comparisons
# -------------------------
run_compare "LSI map 0" \
  "${base_dir}/results_lsi/optix_lsi_map_0.csv" \
  "${base_dir}/results_lsi/vulkan_lsi_map_0.csv"

# Add map 1 too if LSI is dumped per map.
# run_compare "LSI map 1" ...

# -------------------------
# Output polygon comparisons
# -------------------------
#run_compare "Output polygons" \
#  "${base_dir}/results_output/optix_output.csv" \
#  "${base_dir}/results_output/vulkan_output.csv"

exit "$status"