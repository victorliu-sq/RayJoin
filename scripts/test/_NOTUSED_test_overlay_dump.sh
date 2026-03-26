#!/usr/bin/env bash
set -u

status=0
cmp_script="./scripts/test/compare_csv.sh"

run_compare() {
  local label="$1"
  local f1="$2"
  local f2="$3"

  echo "========================================"
  echo "Test: $label"
  echo "  file 1: $f1"
  echo "  file 2: $f2"
  echo "========================================"

  if [[ ! -x "$cmp_script" ]]; then
    echo "Result: FAIL"
    echo "Reason: compare script missing or not executable: $cmp_script"
    status=1
    echo
    return
  fi

  if [[ ! -f "$f1" ]]; then
    echo "Result: FAIL"
    echo "Reason: missing file: $f1"
    status=1
    echo
    return
  fi

  if [[ ! -f "$f2" ]]; then
    echo "Result: FAIL"
    echo "Reason: missing file: $f2"
    status=1
    echo
    return
  fi

  if "$cmp_script" "$f1" "$f2"; then
    echo "Result: PASS"
  else
    echo "Result: FAIL"
    status=1
  fi

  echo
}

# -------------------------
# PIP comparisons
# -------------------------
run_compare "PIP map 0" \
  tmp/results_pip/optix_pip_map_0.csv \
  tmp/results_pip/vulkan_pip_map_0.csv

run_compare "PIP map 1" \
  tmp/results_pip/optix_pip_map_1.csv \
  tmp/results_pip/vulkan_pip_map_1.csv

# -------------------------
# LSI comparisons
# -------------------------
run_compare "LSI map 0" \
  tmp/results_lsi/optix_lsi_map_0.csv \
  tmp/results_lsi/vulkan_lsi_map_0.csv

exit "$status"