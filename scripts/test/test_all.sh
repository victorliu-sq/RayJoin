#!/usr/bin/env bash
set -u

status=0

run_compare() {
  local f1="$1"
  local f2="$2"

  echo "========================================"
  echo "Comparing files:"
  echo "  file 1: $f1"
  echo "  file 2: $f2"
  echo "========================================"

  if ./scripts/test/test_pip.sh "$f1" "$f2"; then
    echo "Result: PASS"
  else
    echo "Result: FAIL"
    status=1
  fi

  echo
}

run_compare tmp/results_pip/optix_pip_map_0.csv tmp/results_pip/vulkan_pip_map_0.csv
run_compare tmp/results_pip/optix_pip_map_1.csv tmp/results_pip/vulkan_pip_map_1.csv

exit "$status"