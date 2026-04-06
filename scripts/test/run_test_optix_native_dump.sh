#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"

poly1="data/realworld_small/br_county_clean_25_odyssey_final.txt"
poly2="data/realworld_small/br_soil_ascii_odyssey_final.txt"

native_output="${base_dir}/results/br_countyXbr_soil_result_native.txt"

mkdir -p "${base_dir}/results"

./build/bin/polyover_exec_native \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$native_output" \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
  -dump_results=map \
  -dump_dir="$base_dir"

points_dir="${base_dir}/results_points"
edges_dir="${base_dir}/results_edges"

echo "Native run completed."
echo "Result file:"
echo "  ${native_output}"
echo "Dumped point files:"
echo "  ${points_dir}/native_points_map_0.csv"
echo "  ${points_dir}/native_points_map_1.csv"
echo "Dumped edge files:"
echo "  ${edges_dir}/native_edges_map_0.csv"
echo "  ${edges_dir}/native_edges_map_1.csv"