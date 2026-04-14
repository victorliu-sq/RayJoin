#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"
cache_dir="${base_dir}/serialized_maps"

poly1="data/realworld_small/br_county_clean_25_odyssey_final.txt"
poly2="data/realworld_small/br_soil_ascii_odyssey_final.txt"

vk_output="${base_dir}/results/br_countyXbr_soil_result_vk_native.txt"
native_output="${base_dir}/results/br_countyXbr_soil_result_native.txt"

mkdir -p "${base_dir}/results"

AG_FLAG=2

# =========================================================
# Run Vulkan native/no-scaling with dumps enabled
./build/bin/polyover_vk_exec_ns \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$vk_output" \
  -mode=rt \
  -ag="${AG_FLAG}" \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=index,lsi,pip,pipmid \
  -dump_dir="$base_dir"

# =========================================================
# Run OptiX native/no-scaling with dumps enabled
./build/bin/polyover_exec_native \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$native_output" \
  -mode=rt \
  -ag="${AG_FLAG}" \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=index,lsi,pip,pipmid \
  -dump_dir="$base_dir"

status=0

index_dir="${base_dir}/results_index"
lsi_dir="${base_dir}/results_lsi"
pip_dir="${base_dir}/results_pip"
mid_dir="${base_dir}/results_mid"
results_dir="${base_dir}/results"

index_diff_dir="${index_dir}/diffs"
lsi_diff_dir="${lsi_dir}/diffs"
pip_diff_dir="${pip_dir}/diffs"
mid_diff_dir="${mid_dir}/diffs"
results_diff_dir="${results_dir}/diffs"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/compare_files.sh"

if [[ "${AG_FLAG}" != "1" ]]; then
  run_compare "Index map 0" \
    "${index_dir}/vulkan_index_map_0.csv" \
    "${index_dir}/native_index_map_0.csv" \
    "$index_diff_dir" || status=1

  run_compare "Index map 1" \
    "${index_dir}/vulkan_index_map_1.csv" \
    "${index_dir}/native_index_map_1.csv" \
    "$index_diff_dir" || status=1
else
  echo "Skipping index comparisons because AG_FLAG=${AG_FLAG}"
fi

run_compare "LSI" \
  "${lsi_dir}/vulkan_lsi.csv" \
  "${lsi_dir}/native_lsi.csv" \
  "$lsi_diff_dir" || status=1

run_compare "PIP map 0" \
  "${pip_dir}/vulkan_pip_map_0.csv" \
  "${pip_dir}/native_pip_map_0.csv" \
  "$pip_diff_dir" || status=1

run_compare "PIP map 1" \
  "${pip_dir}/vulkan_pip_map_1.csv" \
  "${pip_dir}/native_pip_map_1.csv" \
  "$pip_diff_dir" || status=1

run_compare "PIP midpoint map 0" \
  "${mid_dir}/vulkan_pipmid_map_0.csv" \
  "${mid_dir}/native_pipmid_map_0.csv" \
  "$mid_diff_dir" || status=1

run_compare "PIP midpoint map 1" \
  "${mid_dir}/vulkan_pipmid_map_1.csv" \
  "${mid_dir}/native_pipmid_map_1.csv" \
  "$mid_diff_dir" || status=1

run_compare "Final output" \
  "${vk_output}" \
  "${native_output}" \
  "$results_diff_dir" || status=1

exit "$status"