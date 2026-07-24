#!/bin/bash
# Regenerate examples/divine/hsc/ from examples/divine/dve/ and classify every
# model: transform status, then the surface's verdict on the generated file
# (run under a per-model timeout). Writes examples/divine/status.tsv:
#   model <TAB> status <TAB> states-or-detail <TAB> nodes <TAB> seconds
# status: run-ok (states, final diagram nodes), timeout, refused-crossing,
# refused-other, transform-error, overflow/other-error. Every sweep is also
# archived under examples/divine/runs/<utc>_<rev>[_<label>].tsv (a # header
# records revision, timeout, dve2hsc flags) for post analysis; status.tsv
# is the latest view.
# Usage: tests/dve_sweep.sh [timeout-seconds] [label] [dve2hsc flags…]

set -u
cd "$(dirname "$0")/.."
TMO="${1:-5}"
LABEL="${2:-}"
shift $(( $# > 2 ? 2 : $# ))
DVE2HSC=build/tools/dve2hsc
HSCRUN=build/tools/hsc
OUT=examples/divine/status.tsv
LOG=tests/logs/dve_sweep.log
REV=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
STAMP=$(date -u +%Y%m%dT%H%M%SZ)
ARCHIVE=examples/divine/runs/${STAMP}_${REV}${LABEL:+_$LABEL}.tsv
mkdir -p tests/logs examples/divine/hsc examples/divine/runs
{
  echo "# dve_sweep $STAMP rev=$REV timeout=${TMO}s dve2hsc-flags='$*'"
  echo "# model	status	states-or-detail	nodes	seconds"
} > "$ARCHIVE"
: > "$LOG"

for f in examples/divine/dve/*.dve; do
  b=$(basename "$f" .dve)
  hsc=examples/divine/hsc/$b.hsc
  if ! msg=$("$DVE2HSC" "$f" -o "$hsc" "$@" 2>&1); then
    rm -f "$hsc"
    printf '%s\ttransform-error\t%s\t\t\n' "$b" \
      "$(echo "$msg" | sed 's/.*transform error: //' | head -c 100)" >> "$ARCHIVE"
    continue
  fi
  t0=$(date +%s.%N)
  run=$(timeout "$TMO" "$HSCRUN" "$hsc" 2>&1)
  rc=$?
  secs=$(echo "$(date +%s.%N) $t0" | awk '{printf "%.2f", $1 - $2}')
  echo "== $b rc=$rc ${secs}s" >> "$LOG"; echo "$run" >> "$LOG"
  if [ $rc -eq 0 ]; then
    count=$(echo "$run" | sed -n 's/^R count //p')
    nodes=$(echo "$run" | sed -n 's/^R nodes //p')
    printf '%s\trun-ok\t%s\t%s\t%s\n' "$b" "$count" "$nodes" "$secs" >> "$ARCHIVE"
  elif [ $rc -eq 124 ]; then
    printf '%s\ttimeout\t%ss\t\t%s\n' "$b" "$TMO" "$secs" >> "$ARCHIVE"
  elif echo "$run" | grep -q "split_equiv"; then
    printf '%s\trefused-crossing\t%s\t\t%s\n' "$b" \
      "$(echo "$run" | head -1 | sed 's/.*: //' | head -c 100)" "$secs" >> "$ARCHIVE"
  elif echo "$run" | grep -qi "overflow"; then
    printf '%s\toverflow\t\t\t%s\n' "$b" "$secs" >> "$ARCHIVE"
  elif [ $rc -eq 2 ]; then
    printf '%s\trefused-other\t%s\t\t%s\n' "$b" \
      "$(echo "$run" | head -1 | sed 's/.*: //' | head -c 100)" "$secs" >> "$ARCHIVE"
  else
    printf '%s\tother-error\trc=%s\t\t%s\n' "$b" "$rc" "$secs" >> "$ARCHIVE"
  fi
done

grep -v '^#' "$ARCHIVE" > "$OUT"
echo "archived: $ARCHIVE"
grep -v '^#' "$ARCHIVE" | cut -f2 | sort | uniq -c | sort -rn
