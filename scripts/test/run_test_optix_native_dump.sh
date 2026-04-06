#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"

poly1="data/realworld_small/br_county_clean_25_odyssey_final.txt"
poly2="data/realworld_small/br_soil_ascii_odyssey_final.txt"

native_output="${base_dir}/results/br_countyXbr_soil_result_native.txt"
optix_output="${base_dir}/results/br_countyXbr_soil_result_optix.txt"

mkdir -p "${base_dir}/results"

AG_FLAG=0

# =========================================================
# Run Native with index + lsi + pip dumps enabled
./build/bin/polyover_exec_native \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$native_output" \
  -mode=rt \
  -ag="${AG_FLAG}" \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=index,lsi,pip \
  -dump_dir="$base_dir"

# =========================================================
# Run original OptiX with index + lsi + pip dumps enabled
./build/bin/polyover_exec \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$optix_output" \
  -mode=rt \
  -ag="${AG_FLAG}" \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=index,lsi,pip \
  -dump_dir="$base_dir"

status=0

index_dir="${base_dir}/results_index"
lsi_dir="${base_dir}/results_lsi"
pip_dir="${base_dir}/results_pip"

index_diff_dir="${index_dir}/diffs"
lsi_diff_dir="${lsi_dir}/diffs"
pip_diff_dir="${pip_dir}/diffs"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/compare_files.sh"

# -------------------------
# BuildIndex comparisons
# -------------------------
run_compare "Index map 0" \
  "${index_dir}/optix_index_map_0.csv" \
  "${index_dir}/native_index_map_0.csv" \
  "$index_diff_dir" || status=1

run_compare "Index map 1" \
  "${index_dir}/optix_index_map_1.csv" \
  "${index_dir}/native_index_map_1.csv" \
  "$index_diff_dir" || status=1

# -------------------------
# LSI comparisons
# -------------------------
run_compare "LSI" \
  "${lsi_dir}/optix_lsi.csv" \
  "${lsi_dir}/native_lsi.csv" \
  "$lsi_diff_dir" || status=1

# -------------------------
# PIP comparisons
# -------------------------
run_compare "PIP map 0" \
  "${pip_dir}/optix_pip_map_0.csv" \
  "${pip_dir}/native_pip_map_0.csv" \
  "$pip_diff_dir" || status=1

run_compare "PIP map 1" \
  "${pip_dir}/optix_pip_map_1.csv" \
  "${pip_dir}/native_pip_map_1.csv" \
  "$pip_diff_dir" || status=1

exit "$status"