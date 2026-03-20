#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <optix_csv> <vulkan_csv>"
  exit 1
fi

file1="$1"
file2="$2"

if [[ ! -f "$file1" ]]; then
  echo "Error: file not found: $file1"
  exit 1
fi

if [[ ! -f "$file2" ]]; then
  echo "Error: file not found: $file2"
  exit 1
fi

tmp1=$(mktemp)
tmp2=$(mktemp)
trap 'rm -f "$tmp1" "$tmp2"' EXIT

# Remove header, sort by map_id then point_id numerically
tail -n +2 "$file1" | sort -t, -k1,1n -k2,2n > "$tmp1"
tail -n +2 "$file2" | sort -t, -k1,1n -k2,2n > "$tmp2"

if diff -u "$tmp1" "$tmp2" > /dev/null; then
  echo "Equal: CSV contents match."
  exit 0
else
  echo "Not equal: CSV contents differ."
  diff -u "$tmp1" "$tmp2" || true
  exit 2
fi