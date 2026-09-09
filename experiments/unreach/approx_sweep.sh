#!/usr/bin/env bash
# Refutation of the reachability examinations (RC, RF) by over-approximation,
# on the local corpus, three engines side by side:
#   hsc      hsc-pn --approx 5 --approx-only              (S from flows, zeros, safe tag)
#   hscu     hsc-pn --approx 5 --approx-only --approx-units (S with the NUPN unit constraints)
#   lp       petri64 --lp --lpTime 5                     (PetriSpot's state equation, rational)
#   hscb     hscu + --approx-back 50 --approx-back-time 2 (the backward search inside S)
# ENGINES selects them (default "hsc hscu lp").
# Every run is capped at $CAP seconds and $MEMKB kB of virtual memory (6 GB);
# PARALLEL defaults to 4 — eight runs at once plus another job overran a 64 GB
# machine once. Outputs under tests/logs/unreach/$TAG/,
# one <model>-<EXAM>.<engine>.{out,err} pair per run, then a rows file scored
# against the contest oracles: TAG model exam engine total answered correct
# wrong unchecked seconds wrong-names.
#
#   approx_sweep.sh TAG MODELS.txt [PARALLEL=4]
set -u
TAG=${1:?tag}; LIST=${2:?models list}; PAR=${3:-4}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CAP=${CAP:-60}
MEMKB=${MEMKB:-6000000}
INPUTS=$ROOT/tests/logs/mcc2026/INPUTS
ORACLE=/data/ythierry/MCC26deploy/MCC-drivers/oracle
HSC=$ROOT/build/tools/hsc-pn
LP=${LP:-$HOME/git/PetriSpot/build/petri64}
OUT=$ROOT/tests/logs/unreach/$TAG
mkdir -p "$OUT"
ENGINES=${ENGINES:-hsc hscu lp}
export ROOT CAP MEMKB INPUTS ORACLE HSC LP OUT TAG ENGINES

one() {  # model exam
  local m=$1 ex=$2 X props
  X=$([ "$ex" = RC ] && echo ReachabilityCardinality || echo ReachabilityFireability)
  props=$INPUTS/$m/$X.xml
  [ -f "$props" ] || return 0
  local eng cmd t0 t1
  for eng in $ENGINES; do
    case $eng in
      hsc)  cmd=("$HSC" -i "$INPUTS/$m/model.pnml" --props "$props" --shape sloan --approx 5 --approx-only -q) ;;
      hscu) cmd=("$HSC" -i "$INPUTS/$m/model.pnml" --props "$props" --shape sloan --approx 5 --approx-only --approx-units -q) ;;
      lp)   cmd=("$LP" -i "$INPUTS/$m/model.pnml" "--props=$props" --lp --lpTime 5) ;;
      hscb) cmd=("$HSC" -i "$INPUTS/$m/model.pnml" --props "$props" --shape sloan --approx 5 --approx-only --approx-units --approx-back 50 --approx-back-time 2 -q) ;;
    esac
    t0=$(date +%s.%N)
    (ulimit -v "$MEMKB"; timeout "$CAP" "${cmd[@]}") > "$OUT/$m-$ex.$eng.out" 2> "$OUT/$m-$ex.$eng.err"
    local rc=$?
    t1=$(date +%s.%N)
    local secs; secs=$(python3 -c "print(round($t1-$t0,2))")
    python3 "$ROOT/experiments/unreach/score.py" "$ORACLE/$m-$ex.out" "$OUT/$m-$ex.$eng.out" --tsv "$TAG	$m	$ex	$eng" \
      | awk -v s="$secs" -v rc="$rc" -F'\t' 'BEGIN{OFS="\t"}{print $1,$2,$3,$4,$5,$6,$7,$8,$9,s,rc,$10}' >> "$OUT/rows.$$.tsv"
  done
}
export -f one
grep -v '^#' "$LIST" | grep . | while read -r m; do echo "$m RC"; echo "$m RF"; done \
  | xargs -P "$PAR" -L 1 bash -c 'one "$@"' _
cat "$OUT"/rows.*.tsv > "$OUT/rows.tsv" 2>/dev/null
echo "rows: $(wc -l < "$OUT/rows.tsv") in $OUT/rows.tsv"
