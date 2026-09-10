#!/usr/bin/env bash
# The forward gfp over the invariant set, per model: after the dead-transition
# tests, one image of S under the live transitions and the greatest fixpoint
# of X ↦ X ∩ (init ∪ post(X)) below S (`hsc-pn --dead 5 --dead-step --dead-gfp 0`),
# against the reachable set's size from the StateSpace oracle. One row per
# model: model, covered/places, |S|, S nodes, image seconds, gfp verdict,
# rounds, |G|, G nodes, gfp seconds, |R| (oracle, ? when unknown), exit code.
#
#   gfp_sweep.sh TAG MODELS.txt [PARALLEL=4]
set -u
TAG=${1:?tag}; LIST=${2:?models}; PAR=${3:-4}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CAP=${CAP:-120}; MEMKB=${MEMKB:-6000000}; BUDGET=${BUDGET:-30}
HSC=${HSC:-$ROOT/build-b/tools/hsc-pn}
INPUTS=$ROOT/tests/logs/mcc2026/INPUTS
ORACLE=/data/ythierry/MCC26deploy/MCC-drivers/oracle
OUT=$ROOT/tests/logs/unreach/$TAG
mkdir -p "$OUT"
export ROOT CAP MEMKB BUDGET HSC INPUTS ORACLE OUT
one() {
  local m=$1
  (ulimit -v "$MEMKB"; timeout "$CAP" "$HSC" -i "$INPUTS/$m/model.pnml" --shape sloan --dead 5 --dead-step --dead-budget "$BUDGET" --dead-gfp 0 -q) > "$OUT/$m.out" 2> "$OUT/$m.err"
  local rc=$?
  local cov s snodes img gfp r
  cov=$(grep -o "covered=[0-9]*/[0-9]*" "$OUT/$m.err" | head -1 | cut -d= -f2)
  s=$(grep -o "set=[0-9.e+]*" "$OUT/$m.err" | head -1 | cut -d= -f2)
  snodes=$(grep -o "set_nodes=[0-9]*" "$OUT/$m.err" | head -1 | cut -d= -f2)
  img=$(grep "hsc-pn: image" "$OUT/$m.err" | sed 's/.* in \([0-9.e+-]*\) s/\1/')
  gfp=$(grep "hsc-pn: gfp support" "$OUT/$m.err" | sed 's/hsc-pn: gfp support \([a-z]*\) \([0-9]*\) \([0-9.e+]*\) \([0-9]*\) in \([0-9.e+-]*\) s/\1\t\2\t\3\t\4\t\5/')
  r=$(grep "STATE_SPACE STATES" "$ORACLE/$m-SS.out" 2>/dev/null | awk '{print $3}' | head -1)
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$m" "${cov:-?}" "${s:-?}" "${snodes:-?}" "${img:-?}" "${gfp:-?	?	?	?	?}" "${r:-?}" "$rc" >> "$OUT/rows.$$.tsv"
}
export -f one
grep -v '^#' "$LIST" | grep . | xargs -P "$PAR" -I{} bash -c 'one {}'
cat "$OUT"/rows.*.tsv > "$OUT/rows.tsv"
echo "rows: $(wc -l < "$OUT/rows.tsv") in $OUT/rows.tsv"
