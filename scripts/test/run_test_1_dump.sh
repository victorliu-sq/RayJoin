#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"

poly1="data/realworld_small/br_county_clean_25_odyssey_final.txt"
poly2="data/realworld_small/br_soil_ascii_odyssey_final.txt"

vk_output="${base_dir}/results/br_countyXbr_soil_result_vk.txt"
optix_output="${base_dir}/results/br_countyXbr_soil_result_optix.txt"

# =========================================================
# Run Vulkan with dumps enabled
./build/bin/polyover_vk_exec \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$vk_output" \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=map,pip,lsi,pipmid \
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
  -dump_results=map,pip,lsi,pipmid \
  -dump_dir="$base_dir"

status=0

scaling_dir="${base_dir}/results_scaling"
edges_dir="${base_dir}/results_edges"
pip_dir="${base_dir}/results_pip"
lsi_dir="${base_dir}/results_lsi"
mid_dir="${base_dir}/results_mid"

scaling_diff_dir="${scaling_dir}/diffs"
edges_diff_dir="${edges_dir}/diffs"
pip_diff_dir="${pip_dir}/diffs"
lsi_diff_dir="${lsi_dir}/diffs"
mid_diff_dir="${mid_dir}/diffs"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/compare_files.sh"

# -------------------------
# Scaling comparisons
# -------------------------
run_compare "Scaling map 0" \
  "${scaling_dir}/optix_scaling_map_0.csv" \
  "${scaling_dir}/vulkan_scaling_map_0.csv" \
  "$scaling_diff_dir" || status=1

run_compare "Scaling map 1" \
  "${scaling_dir}/optix_scaling_map_1.csv" \
  "${scaling_dir}/vulkan_scaling_map_1.csv" \
  "$scaling_diff_dir" || status=1


# -------------------------
# Edge comparisons
# -------------------------
run_compare "Edges map 0" \
  "${edges_dir}/optix_edges_map_0.csv" \
  "${edges_dir}/vulkan_edges_map_0.csv" \
  "$edges_diff_dir" || status=1

run_compare "Edges map 1" \
  "${edges_dir}/optix_edges_map_1.csv" \
  "${edges_dir}/vulkan_edges_map_1.csv" \
  "$edges_diff_dir" || status=1

# -------------------------
# PIP comparisons
# -------------------------
#run_compare "PIP map 0" \
#  "${pip_dir}/optix_pip_map_0.csv" \
#  "${pip_dir}/vulkan_pip_map_0.csv" \
#  "$pip_diff_dir" || status=1
#
#run_compare "PIP map 1" \
#  "${pip_dir}/optix_pip_map_1.csv" \
#  "${pip_dir}/vulkan_pip_map_1.csv" \
#  "$pip_diff_dir" || status=1

# -------------------------
# LSI comparisons
# -------------------------
#run_compare "LSI map 0" \
#  "${lsi_dir}/optix_lsi_map_0.csv" \
#  "${lsi_dir}/vulkan_lsi_map_0.csv" \
#  "$lsi_diff_dir" || status=1

# -------------------------
# Midpoint PIP comparisons
# -------------------------
#run_compare "PIP midpoint map 0" \
#  "${mid_dir}/optix_pipmid_map_0.csv" \
#  "${mid_dir}/vulkan_pipmid_map_0.csv" \
#  "$mid_diff_dir" || status=1
#
#run_compare "PIP midpoint map 1" \
#  "${mid_dir}/optix_pipmid_map_1.csv" \
#  "${mid_dir}/vulkan_pipmid_map_1.csv" \
#  "$mid_diff_dir" || status=1

exit "$status"