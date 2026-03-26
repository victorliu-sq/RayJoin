#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"

poly1="data/realworld_small/br_county_clean_25_odyssey_final.txt"
poly2="data/realworld_small/br_soil_ascii_odyssey_final.txt"

vk_output="${base_dir}/results/br_countyXbr_soil_result_vk.txt"
optix_output="${base_dir}/results/br_countyXbr_soil_result_optix.txt"

# =========================================================
# Run Vulkan with dumps enabled
./build/bin/polyover_vk_exec_ns \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$vk_output" \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=pip,lsi,output \
  -dump_dir="$base_dir"

# =========================================================
# Run OptiX with dumps enabled
./build/bin/polyover_exec \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$optix_output" \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=pip,lsi,output \
  -dump_dir="$base_dir"

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

pip_dir="${base_dir}/results_pip"
lsi_dir="${base_dir}/results_lsi"
output_dir="${base_dir}/results_output"

# -------------------------
# PIP comparisons
# -------------------------
run_compare "PIP map 0" \
  "${pip_dir}/optix_pip_map_0.csv" \
  "${pip_dir}/vulkan_pip_map_0.csv"

run_compare "PIP map 1" \
  "${pip_dir}/optix_pip_map_1.csv" \
  "${pip_dir}/vulkan_pip_map_1.csv"

# -------------------------
# LSI comparisons
# -------------------------
run_compare "LSI map 0" \
  "${lsi_dir}/optix_lsi_map_0.csv" \
  "${lsi_dir}/vulkan_lsi_map_0.csv"

# -------------------------
# Output polygon comparisons
# -------------------------
# run_compare "Output polygons" \
#   "${output_dir}/optix_output.csv" \
#   "${output_dir}/vulkan_output.csv"

exit "$status"