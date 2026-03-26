#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 <file1> <file2> <diff_dir>"
  exit 1
fi

file1="$1"
file2="$2"
diff_dir="$3"

if [[ ! -f "$file1" ]]; then
  echo "Error: file not found: $file1"
  exit 1
fi

if [[ ! -f "$file2" ]]; then
  echo "Error: file not found: $file2"
  exit 1
fi

mkdir -p "$diff_dir"

name1=$(basename "$file1")
name2=$(basename "$file2")

name1="${name1%.*}"
name2="${name2%.*}"

diff_file="${diff_dir}/${name1}__vs__${name2}.diff"

if diff -u "$file1" "$file2" > "$diff_file"; then
  rm -f "$diff_file"
  echo "Equal: file contents match."
  exit 0
else
  echo "Not equal: file contents differ."
  echo "Diff saved to: $diff_file"
  exit 2
fi