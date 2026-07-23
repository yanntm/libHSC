#!/bin/bash
# Run the libITS reference (~/git/libITS/bin/its-reach, DVE front end) over
# samples/divine/dve/ and record one line per model in
# tests/logs/its_reach_counts.tsv:  model <TAB> count | timeout | error.
# The current authoritative oracle: tests/compare_beem_baseline.sh prefers
# it over the historical extraction (beem_baseline_counts.tsv).
# Usage: tests/collect_its_baseline.sh [per-model-timeout-seconds] (default 15)
set -u
cd "$(dirname "$0")/.."
TMO="${1:-15}"
ITS=~/git/libITS/bin/its-reach
OUT=tests/logs/its_reach_counts.tsv
: > "$OUT"

for f in samples/divine/dve/*.dve; do
  b=$(basename "$f" .dve)
  run=$(timeout "$TMO" "$ITS" -t DVE --quiet -i "$f" 2>&1)
  rc=$?
  count=$(echo "$run" | sed -n 's/.*Total reachable state count : //p' | head -1)
  if [ -n "$count" ]; then
    printf '%s\t%s\n' "$b" "$count" >> "$OUT"
  elif [ $rc -eq 124 ]; then
    printf '%s\ttimeout\n' "$b" >> "$OUT"
  else
    printf '%s\terror\n' "$b" >> "$OUT"
  fi
done
