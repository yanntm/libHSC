#!/bin/bash
# Regenerate samples/divine/hsc/ from samples/divine/dve/ and classify every
# model: transform status, then the surface's verdict on the generated file
# (run under a per-model timeout). Writes samples/divine/status.tsv:
#   model <TAB> status [<TAB> detail]
# status: run-ok (detail: the state count), refused-§7 (crossing form),
# refused-other, timeout, transform-error, overflow/other-error.
# Usage: tests/dve_sweep.sh [per-model-timeout-seconds]   (default 5)

set -u
cd "$(dirname "$0")/.."
TMO="${1:-5}"
DVE2HSC=build/tools/dve2hsc
HSCRUN=build/examples/hscrun
OUT=samples/divine/status.tsv
LOG=tests/logs/dve_sweep.log
mkdir -p tests/logs samples/divine/hsc
: > "$OUT"
: > "$LOG"

for f in samples/divine/dve/*.dve; do
  b=$(basename "$f" .dve)
  hsc=samples/divine/hsc/$b.hsc
  if ! msg=$("$DVE2HSC" "$f" -o "$hsc" 2>&1); then
    rm -f "$hsc"
    printf '%s\ttransform-error\t%s\n' "$b" \
      "$(echo "$msg" | sed 's/.*transform error: //' | head -c 100)" >> "$OUT"
    continue
  fi
  run=$(timeout "$TMO" "$HSCRUN" "$hsc" 2>&1)
  rc=$?
  echo "== $b rc=$rc" >> "$LOG"; echo "$run" >> "$LOG"
  if [ $rc -eq 0 ]; then
    count=$(echo "$run" | sed -n 's/^R count //p')
    printf '%s\trun-ok\t%s\n' "$b" "$count" >> "$OUT"
  elif [ $rc -eq 124 ]; then
    printf '%s\ttimeout\t%ss\n' "$b" "$TMO" >> "$OUT"
  elif echo "$run" | grep -q "§7"; then
    printf '%s\trefused-§7\t%s\n' "$b" \
      "$(echo "$run" | head -1 | sed 's/.*: //' | head -c 100)" >> "$OUT"
  elif echo "$run" | grep -qi "overflow"; then
    printf '%s\toverflow\n' "$b" >> "$OUT"
  elif [ $rc -eq 2 ]; then
    printf '%s\trefused-other\t%s\n' "$b" \
      "$(echo "$run" | head -1 | sed 's/.*: //' | head -c 100)" >> "$OUT"
  else
    printf '%s\tother-error\trc=%s\n' "$b" "$rc" >> "$OUT"
  fi
done

cut -f2 "$OUT" | sort | uniq -c | sort -rn
