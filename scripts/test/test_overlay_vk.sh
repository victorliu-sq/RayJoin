#!/usr/bin/env bash

set -u

file1="tmp/results/br_countyXbr_soil_result_optix.txt"
file2="tmp/results/br_countyXbr_soil_result_vk.txt"
diff_file="tmp/results/diff.txt"

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
if diff "$file1" "$file2" > "$diff_file"; then
    echo "Test passed"
    rm -f "$diff_file"
    exit 0
else
    status=$?

    if [[ $status -eq 1 ]]; then
        echo "Test failed"
        echo "Diff:"
        cat "$diff_file"
        exit 1
    else
        echo "Error: diff command failed" >&2
        exit 2
    fi
fi