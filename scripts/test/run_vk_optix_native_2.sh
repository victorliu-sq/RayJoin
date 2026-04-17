#!/usr/bin/env bash
set -euo pipefail

base_dir="tmp"
cache_dir="${base_dir}/serialized_maps"

poly1="data/realworld/Aquifers.cdb"
poly2="data/realworld/dtl_cnty.cdb"

vk_output="${base_dir}/results/Aquifers_Cnty_result_vk_native.txt"
native_output="${base_dir}/results/Aquifers_Cnty_result_native.txt"

mkdir -p "${base_dir}/results"
mkdir -p "${cache_dir}"

AG_FLAG=2

# =========================================================
# Run Vulkan native/no-scaling with dumps enabled
./build/bin/polyover_vk_exec_ns \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$vk_output" \
  -mode=rt \
  -serialize="$cache_dir" \
  -ag="${AG_FLAG}" \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
#  -dump_results=index,lsi,pip,pipmid \
#  -dump_dir="$base_dir"

# =========================================================
# Run OptiX native with dumps enabled
./build/bin/polyover_exec_native \
  -poly1 "$poly1" \
  -poly2 "$poly2" \
  -output "$native_output" \
  -mode=rt \
  -serialize="$cache_dir" \
  -ag="${AG_FLAG}" \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip \
#  -dump_results=index,lsi,pip,pipmid \
#  -dump_dir="$base_dir" \

status=0
exit "$status"
