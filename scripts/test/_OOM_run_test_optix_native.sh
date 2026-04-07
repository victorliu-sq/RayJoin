#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"
cache_dir="${base_dir}/serialized_maps"

poly1="data/realworld/dtl_cnty.cdb"
poly2="data/realworld/USAZIPCodeArea.cdb"

native_output="${base_dir}/results/Cnty_Zipcode_result_native.txt"
optix_output="${base_dir}/results/Cnty_Zipcode_result_optix.txt"

mkdir -p "${base_dir}/results"
mkdir -p "${cache_dir}"

AG_FLAG=0

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
  -dump_dir="$base_dir" \
  -serialize="$cache_dir"

./build/bin/polyover_exec \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$optix_output" \
  -mode=rt \
  -ag="${AG_FLAG}" \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=index,lsi,pip,pipmid \
  -dump_dir="$base_dir" \
  -serialize="$cache_dir"

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

run_compare "Index map 0" \
  "${index_dir}/optix_index_map_0.csv" \
  "${index_dir}/native_index_map_0.csv" \
  "$index_diff_dir" || status=1

run_compare "Index map 1" \
  "${index_dir}/optix_index_map_1.csv" \
  "${index_dir}/native_index_map_1.csv" \
  "$index_diff_dir" || status=1

run_compare "LSI" \
  "${lsi_dir}/optix_lsi.csv" \
  "${lsi_dir}/native_lsi.csv" \
  "$lsi_diff_dir" || status=1

run_compare "PIP map 0" \
  "${pip_dir}/optix_pip_map_0.csv" \
  "${pip_dir}/native_pip_map_0.csv" \
  "$pip_diff_dir" || status=1

run_compare "PIP map 1" \
  "${pip_dir}/optix_pip_map_1.csv" \
  "${pip_dir}/native_pip_map_1.csv" \
  "$pip_diff_dir" || status=1

run_compare "PIP midpoint map 0" \
  "${mid_dir}/optix_pipmid_map_0.csv" \
  "${mid_dir}/native_pipmid_map_0.csv" \
  "$mid_diff_dir" || status=1

run_compare "PIP midpoint map 1" \
  "${mid_dir}/optix_pipmid_map_1.csv" \
  "${mid_dir}/native_pipmid_map_1.csv" \
  "$mid_diff_dir" || status=1

run_compare "Final output" \
  "${optix_output}" \
  "${native_output}" \
  "$results_diff_dir" || status=1

exit "$status"