#!/bin/bash
# Sweep the BEEM corpus through the cegar bridge and classify acceptance.
# For each examples/divine/hsc/M.hsc: keep the declarations/events (strip
# engine commands), append (declare-domains) and a trivial one-leaf cegar
# probe, run under a per-model timeout, and classify the outcome:
#   ok-violation / ok-holds  — the bridge accepted; the loop ran
#   ok-timeout               — accepted, loop exceeded the budget
#   refused-bound            — a leaf domain defied inference
#   refused-guard            — a guard atom crosses leaves
#   refused-read             — an action rhs reads another leaf
#   refused-array            — a dynamic array access survives the chain
#   refused-other / error    — any other refusal or failure
# Writes one TSV row per model: model, status, detail, seconds.
# Usage: tests/cegar_dve_sweep.sh [timeout-seconds] [out.tsv]
set -u
cd "$(dirname "$0")/.."
TMO="${1:-15}"
OUT="${2:-tests/logs/cegar_dve_accept.tsv}"
HSC=build/tools/hsc
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
: > "$OUT"

for f in examples/divine/hsc/*.hsc; do
  m=$(basename "$f" .hsc)
  p="$WORK/$m.hsc"
  sed '/^(reach/,$d' "$f" > "$p"
  # property leaf: the first scalar leaf with a non-constant domain (a
  # constant leaf is elided by simplify-constants and cannot carry the atom)
  "$HSC" --domains "$p" 2>/dev/null | awk \
    '/^xdom /{if (!($0~/kind=set/ && $0~/ size=1 /) && !($0~/kind=top/))
       print $2}' > "$WORK/ok-units"
  leaf=$(grep '^(leaf' "$p" | awk '{print $2}' | tr -d ')' |
         grep -m1 -Fxf "$WORK/ok-units")
  if [ -z "$leaf" ]; then
    printf '%s\terror\tno-usable-leaf\t0\n' "$m" >> "$OUT"; continue
  fi
  printf '(declare-domains)\n(cegar v (== %s 1))\n' "$leaf" >> "$p"
  t0=$(date +%s.%N)
  outp=$(timeout "$TMO" "$HSC" "$p" 2>&1)
  rc=$?
  t1=$(date +%s.%N)
  secs=$(echo "$t1 $t0" | awk '{printf "%.2f", $1-$2}')
  detail=""
  if [ $rc -eq 124 ]; then
    status=ok-timeout
  elif echo "$outp" | grep -q 'cegar violation'; then
    status=ok-violation
    detail=$(echo "$outp" | grep -o 'rounds [0-9]*' | head -1)
  elif echo "$outp" | grep -q 'cegar holds'; then
    status=ok-holds
    detail=$(echo "$outp" | grep -o 'rounds [0-9]*' | head -1)
  else
    line=$(echo "$outp" | grep -m1 'cegar:')
    detail=$(echo "$line" | sed 's/.*cegar: //' | cut -c1-90)
    case "$line" in
      *"no declared bound"*)        status=refused-bound ;;
      *"guard atom crosses"*)       status=refused-guard ;;
      *"reads another leaf"*)       status=refused-read ;;
      *array*|*"dynamic"*)          status=refused-array ;;
      *cegar:*)                     status=refused-other ;;
      *) status=error
         detail=$(echo "$outp" | tail -1 | cut -c1-90) ;;
    esac
  fi
  printf '%s\t%s\t%s\t%s\n' "$m" "$status" "$detail" "$secs" >> "$OUT"
done

cut -f2 "$OUT" | sort | uniq -c | sort -rn
