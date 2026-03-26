#!/usr/bin/env bash

run_compare() {
  local label="$1"
  local file1="$2"
  local file2="$3"
  local diff_dir="$4"

  echo "========================================"
  echo "Test: $label"
  echo "  file 1  : $file1"
  echo "  file 2  : $file2"
  echo "  diff dir: $diff_dir"
  echo "========================================"

  if [[ ! -f "$file1" ]]; then
    echo "Result: FAIL"
    echo "Reason: missing file: $file1"
    echo
    return 1
  fi

  if [[ ! -f "$file2" ]]; then
    echo "Result: FAIL"
    echo "Reason: missing file: $file2"
    echo
    return 1
  fi

  mkdir -p "$diff_dir"

  local name1
  local name2
  local diff_file

  name1=$(basename "$file1")
  name2=$(basename "$file2")

  name1="${name1%.*}"
  name2="${name2%.*}"

  diff_file="${diff_dir}/${name1}__vs__${name2}.diff"

  if diff -u "$file1" "$file2" > "$diff_file"; then
    rm -f "$diff_file"
    echo "Result: PASS"
    echo
    return 0
  else
    echo "Result: FAIL"
    echo "Reason: contents differ"
    echo "Diff saved to: $diff_file"
    echo
    return 1
  fi
}