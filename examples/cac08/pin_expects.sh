#!/bin/sh
# Pin driver expectations from a verified sweep record. Reads
# experiments/cegar/cac08_verdicts.tsv (or the given TSV) and, for every
# row where the triangle agreed (status ok, agree yes, certcheck pass on
# holds), rewrites the subject's driver expectations:
#   (expect v 0)   — the cegar result binds no bad state (holds)
#   (expect x N)   — the explicit engine's concrete state count
# Rows that timed out or disagreed pin nothing — the driver stays bare,
# and the TSV is the record of why. Idempotent: existing expect lines
# are replaced. Usage: sh pin_expects.sh [verdicts.tsv]
set -eu
cd "$(dirname "$0")"
TSV=${1:-../../experiments/cegar/cac08_verdicts.tsv}

pinned=0 skipped=0
while IFS="$(printf '\t')" read -r system subject status verdict _sym agree cc \
    _rounds _cex _budget _rungs _inv _abs _wl _syms xstates _rest; do
  [ "$system" = system ] && continue  # header
  drv="hsc/$system/$subject.hsc"
  if [ ! -f "$drv" ]; then
    echo "missing driver $drv" >&2
    exit 1
  fi
  if [ "$status" != ok ] || [ "$agree" != yes ] ||
     { [ "$verdict" = holds ] && [ "$cc" != pass ]; }; then
    skipped=$((skipped + 1))
    continue
  fi
  grep -v '^(expect ' "$drv" > "$drv.tmp"
  if [ "$verdict" = holds ]; then
    printf '(expect v 0)\n' >> "$drv.tmp"
  fi
  printf '(expect x %s)\n' "$xstates" >> "$drv.tmp"
  mv "$drv.tmp" "$drv"
  pinned=$((pinned + 1))
done < "$TSV"
echo "pinned $pinned drivers, skipped $skipped rows"
