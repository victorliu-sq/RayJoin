#!/usr/bin/env bash
set -u

# ==================================================================
# Run MapOverlay for Optix and Vulkan
./build/bin/polyover_vk_exec_ns \
  -poly1 data/realworld/Aquifers.cdb \
  -poly2 data/realworld/dtl_cnty.cdb \
  -output  tmp/results/Aquifers_Cnty_result_vk.txt \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip

./build/bin/polyover_exec \
  -poly1 data/realworld/Aquifers.cdb \
  -poly2 data/realworld/dtl_cnty.cdb \
  -output  tmp/results/Aquifers_Cnty_result_optix.txt \
  -mode=rt \
  -xsect_factor=5.0 \
  -warmup=5 -repeat=5 \
  -query=lsi/pip

# ==================================================================
# Test results
file1="tmp/results/Aquifers_Cnty_result_optix.txt"
file2="tmp/results/Aquifers_Cnty_result_vk.txt"
diff_file="tmp/results/test2_diff.txt"

# Check that both files exist
if [[ ! -f "$file1" ]]; then
    echo "Error: file not found: $file1" >&2
    exit 2
fi

if [[ ! -f "$file2" ]]; then
    echo "Error: file not found: $file2" >&2
    exit 2
fi

# Compare files
echo "================================================="
echo "Test-2: Comparing OptiX and Vulkan output files for Realworld Datasets..."

if diff "$file1" "$file2" > "$diff_file"; then
    echo "Test-2 PASS: The output files are identical."
    rm -f "$diff_file"
    exit 0
else
    status=$?

    if [[ $status -eq 1 ]]; then
        echo "Test-2 FAIL: The output files differ."
#        echo "Differences:"
#        cat "$diff_file"
        exit 1
    else
        echo "ERROR: Failed to run the diff command." >&2
        exit 2
    fi
fi