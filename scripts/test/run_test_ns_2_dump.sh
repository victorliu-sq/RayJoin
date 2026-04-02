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
  -dump_results=index,pip,lsi,pipmid \
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
  -dump_results=index,pip,lsi,pipmid \
  -dump_dir="$base_dir"

status=0

index_dir="${base_dir}/results_index"
pip_dir="${base_dir}/results_pip"
lsi_dir="${base_dir}/results_lsi"
mid_dir="${base_dir}/results_mid"
midpoints_dir="${base_dir}/results_midpoints"
midpoint_closest_dir="${base_dir}/results_midpoint_closest"
midpoint_finalize_dir="${base_dir}/results_midpoint_finalize"

index_diff_dir="${index_dir}/diffs"
pip_diff_dir="${pip_dir}/diffs"
lsi_diff_dir="${lsi_dir}/diffs"
mid_diff_dir="${mid_dir}/diffs"
midpoints_diff_dir="${midpoints_dir}/diffs"
midpoint_closest_diff_dir="${midpoint_closest_dir}/diffs"
midpoint_finalize_diff_dir="${midpoint_finalize_dir}/diffs"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/compare_files.sh"

# -------------------------
# BuildIndex comparisons
# -------------------------
run_compare "Index map 0" \
  "${index_dir}/optix_index_map_0.csv" \
  "${index_dir}/vulkan_index_map_0.csv" \
  "$index_diff_dir" || status=1

run_compare "Index map 1" \
  "${index_dir}/optix_index_map_1.csv" \
  "${index_dir}/vulkan_index_map_1.csv" \
  "$index_diff_dir" || status=1

# -------------------------
# LSI comparisons
# -------------------------
run_compare "LSI" \
  "${lsi_dir}/optix_lsi.csv" \
  "${lsi_dir}/vulkan_lsi.csv" \
  "$lsi_diff_dir" || status=1

# -------------------------
# PIP comparisons
# -------------------------
run_compare "PIP map 0" \
  "${pip_dir}/optix_pip_map_0.csv" \
  "${pip_dir}/vulkan_pip_map_0.csv" \
  "$pip_diff_dir" || status=1

run_compare "PIP map 1" \
  "${pip_dir}/optix_pip_map_1.csv" \
  "${pip_dir}/vulkan_pip_map_1.csv" \
  "$pip_diff_dir" || status=1

# -------------------------
# Midpoint sequence comparisons
# -------------------------
run_compare "Sorted midpoints map 0" \
  "${midpoints_dir}/optix_sorted_midpoints_map_0.csv" \
  "${midpoints_dir}/vulkan_sorted_midpoints_map_0.csv" \
  "$midpoints_diff_dir" || status=1

run_compare "Sorted midpoints map 1" \
  "${midpoints_dir}/optix_sorted_midpoints_map_1.csv" \
  "${midpoints_dir}/vulkan_sorted_midpoints_map_1.csv" \
  "$midpoints_diff_dir" || status=1

# -------------------------
# Midpoint closest-eid comparisons
# -------------------------
run_compare "Midpoint closest map 0" \
  "${midpoint_closest_dir}/optix_midpoint_closest_map_0.csv" \
  "${midpoint_closest_dir}/vulkan_midpoint_closest_map_0.csv" \
  "$midpoint_closest_diff_dir" || status=1

run_compare "Midpoint closest map 1" \
  "${midpoint_closest_dir}/optix_midpoint_closest_map_1.csv" \
  "${midpoint_closest_dir}/vulkan_midpoint_closest_map_1.csv" \
  "$midpoint_closest_diff_dir" || status=1

# -------------------------
# Midpoint finalize comparisons
# -------------------------
run_compare "Midpoint finalize map 0" \
  "${midpoint_finalize_dir}/optix_midpoint_finalize_map_0.csv" \
  "${midpoint_finalize_dir}/vulkan_midpoint_finalize_map_0.csv" \
  "$midpoint_finalize_diff_dir" || status=1

run_compare "Midpoint finalize map 1" \
  "${midpoint_finalize_dir}/optix_midpoint_finalize_map_1.csv" \
  "${midpoint_finalize_dir}/vulkan_midpoint_finalize_map_1.csv" \
  "$midpoint_finalize_diff_dir" || status=1

# -------------------------
# Midpoint PIP comparisons
# -------------------------
run_compare "PIP midpoint map 0" \
  "${mid_dir}/optix_pipmid_map_0.csv" \
  "${mid_dir}/vulkan_pipmid_map_0.csv" \
  "$mid_diff_dir" || status=1

run_compare "PIP midpoint map 1" \
  "${mid_dir}/optix_pipmid_map_1.csv" \
  "${mid_dir}/vulkan_pipmid_map_1.csv" \
  "$mid_diff_dir" || status=1

exit "$status"