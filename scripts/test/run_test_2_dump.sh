#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"

poly1="data/realworld/Aquifers.cdb"
poly2="data/realworld/dtl_cnty.cdb"

vk_output="${base_dir}/results/Aquifers_Cnty_result_vk.txt"
optix_output="${base_dir}/results/Aquifers_Cnty_result_optix.txt"

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
file_cmp_script="./scripts/test/compare_files.sh"

pip_dir="${base_dir}/results_pip"
lsi_dir="${base_dir}/results_lsi"
output_dir="${base_dir}/results_output"

pip_diff_dir="${pip_dir}/diffs"
lsi_diff_dir="${lsi_dir}/diffs"
output_diff_dir="${output_dir}/diffs"

run_compare() {
  local label="$1"
  local f1="$2"
  local f2="$3"
  local diff_dir="$4"

  echo "========================================"
  echo "Test: $label"
  echo "  file 1  : $f1"
  echo "  file 2  : $f2"
  echo "  diff dir: $diff_dir"
  echo "========================================"

  if [[ ! -x "$file_cmp_script" ]]; then
    echo "Result: FAIL"
    echo "Reason: compare script missing or not executable: $file_cmp_script"
    status=1
    echo
    return
  fi

  local output
  local rc=0
  output="$("$file_cmp_script" "$f1" "$f2" "$diff_dir" 2>&1)" || rc=$?

  echo "$output"

  if [[ $rc -eq 0 ]]; then
    echo "Result: PASS"
  else
    echo "Result: FAIL"
    if [[ "$output" == *"file not found"* ]]; then
      echo "Reason: missing input file"
    else
      echo "Reason: contents differ"
    fi
    status=1
  fi

  echo
}

# -------------------------
# PIP comparisons
# -------------------------
run_compare "PIP map 0" \
  "${pip_dir}/optix_pip_map_0.csv" \
  "${pip_dir}/vulkan_pip_map_0.csv" \
  "$pip_diff_dir"

run_compare "PIP map 1" \
  "${pip_dir}/optix_pip_map_1.csv" \
  "${pip_dir}/vulkan_pip_map_1.csv" \
  "$pip_diff_dir"

# -------------------------
# LSI comparisons
# -------------------------
run_compare "LSI map 0" \
  "${lsi_dir}/optix_lsi_map_0.csv" \
  "${lsi_dir}/vulkan_lsi_map_0.csv" \
  "$lsi_diff_dir"

# -------------------------
# Output polygon comparisons
# -------------------------
run_compare "Output polygons" \
  "$optix_output" \
  "$vk_output" \
  "$output_diff_dir"

exit "$status"