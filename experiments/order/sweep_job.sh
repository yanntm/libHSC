#!/usr/bin/env bash
# sweep_job.sh: one instance, one examination, every heuristic in sequence,
# a fixed budget each — the unit of the order sweep (SWEEP.md §3, §4).
#   experiments/order/sweep_job.sh <instance> <SS|CTLC|CTLF|RC|RF|RD> [heuristics.tsv]
# Environment: SWEEP_BUDGET (s per run, default 300), SWEEP_MEM (ulimit -v kB,
# default 15000000 — the 15 GB rule; a kill by it is status=memory, a budget spent
# inside the reachable set status=noreach), SWEEP_BIN (hsc-pn), SWEEP_INPUTS
# (folder holding <instance>/model.pnml and the property XML), SWEEP_ORACLE
# (folder of <instance>-<EXAM>.out), SWEEP_OUT (folder for the TSV and logs),
# SWEEP_TAG (a name for this sweep). Appends one TSV line per heuristic to
# $SWEEP_OUT/$SWEEP_TAG.tsv, logs beside as <instance>-<exam>-<heuristic>.{out,err}.
# Idempotent per line: a heuristic whose .out already exists is skipped.
set -u
INST=${1:?instance}; EX=${2:?exam}; HEUR=${3:-$(dirname "$0")/heuristics.tsv}
BUDGET=${SWEEP_BUDGET:-300}; MEM=${SWEEP_MEM:-15000000}
BIN=${SWEEP_BIN:-$(cd "$(dirname "$0")/../.." && pwd)/build/tools/hsc-pn}
INPUTS=${SWEEP_INPUTS:?}; ORACLE=${SWEEP_ORACLE:?}; OUT=${SWEEP_OUT:?}; TAG=${SWEEP_TAG:-sweep}
mkdir -p "$OUT/$TAG"
case $EX in
  CTLC) XML=CTLCardinality;; CTLF) XML=CTLFireability;; RC) XML=ReachabilityCardinality;;
  RF) XML=ReachabilityFireability;; RD) XML=ReachabilityDeadlock;; SS) XML=;; *) echo "unknown exam $EX"; exit 2;;
esac
xml="$INPUTS/$INST/$XML.xml"; orc="$ORACLE/$INST-$EX.out"; pnml="$INPUTS/$INST/model.pnml"
family=${INST%%-PT-*}
states=$(grep -h "STATE_SPACE STATES" "$ORACLE/$INST-SS.out" 2>/dev/null | awk '{print $3}' | head -1)
TSV="$OUT/$TAG.tsv"
[ -f "$TSV" ] || echo -e "instance\tfamily\texam\theuristic\ttag\tstates\twall_s\tcpu_s\tmaxrss_kb\trc\tstatus\tanswered\tok\twrong\tunknown\tcomplete\tanswer_times\treach_s\treach_nodes\treach_arcs\tbelly_nodes\tbelly_level\tbelly_span\tshape_depth\tshape_units\tshape_widest\tflows\tflows_widest\tflows_maxconst\tprotected" > "$TSV"
# StateSpace: `--states` instead of a property file, the four values graded
# against the oracle when it has one (an instance without an SS oracle still
# runs: its values are answered, unknown to the oracle, never wrong).
if [ "$EX" = SS ]; then QUERY="--states"; [ -f "$pnml" ] || { echo -e "$INST\t$family\t$EX\t-\t$TAG\t$states\t\t\t\t\tno-input" >> "$TSV"; exit 0; }
else QUERY="--props $xml"; [ -f "$xml" ] && [ -f "$orc" ] && [ -f "$pnml" ] || { echo -e "$INST\t$family\t$EX\t-\t$TAG\t$states\t\t\t\t\tno-input" >> "$TSV"; exit 0; }; fi
grep -v '^#' "$HEUR" | while IFS=$'\t' read -r name flags envs; do
  [ -n "$name" ] || continue
  base="$OUT/$TAG/$INST-$EX-$name"; out="$base.out"; err="$base.err"; tm="$base.time"
  [ -f "$out" ] && continue
  envs_=""; [ "$envs" != "-" ] && envs_="$envs"
  ( ulimit -v "$MEM"; env $envs_ /usr/bin/time -f "%e %U %S %M" -o "$tm" timeout "$BUDGET" "$BIN" -i "$pnml" $QUERY --totalTime $((BUDGET-2)) --printUnknown -q -v $flags > "$out" 2> "$err" ); rc=$?
  read -r wall usr sys mem < <(tail -1 "$tm" 2>/dev/null)
  cpu=$(awk -v u="${usr:-0}" -v s="${sys:-0}" 'BEGIN{printf "%.2f", u+s}')
  status=ok; [ "$rc" = 124 ] && status=timeout
  grep -q "std::bad_alloc\|Cannot allocate\|out of memory" "$err" 2>/dev/null && status=memory
  [ "$status" = ok ] && [ "$rc" != 0 ] && status=crash
  # the budget ran out inside the reachable set: nothing was answered and no stats line came
  [ "$status" = ok ] && ! grep -q '^hsc-pn: stats ' "$err" 2>/dev/null && status=noreach
  ok=0; wrong=0; unk=0; ans=0
  if [ "$EX" = SS ]; then
    for v in STATES TRANSITIONS MAX_TOKEN_IN_PLACE MAX_TOKEN_PER_MARKING; do
      ours=$(grep -E "^STATE_SPACE $v " "$out" | head -1 | awk '{print $3}')
      ref=$(grep -E "^STATE_SPACE $v " "$orc" 2>/dev/null | head -1 | awk '{print $3}')
      if [ -z "$ours" ]; then unk=$((unk+1));
      elif [ -z "$ref" ] || [ "$ref" = "?" ]; then ans=$((ans+1));
      elif [ "$ours" = "$ref" ]; then ok=$((ok+1)); ans=$((ans+1));
      else wrong=$((wrong+1)); ans=$((ans+1)); fi
    done
  else
  while read -r fname verdict; do
    ours=$(grep -E "^FORMULA $fname " "$out" | head -1 | awk '{print $3}')
    if [ -z "$ours" ]; then unk=$((unk+1));
    elif [ "$verdict" = "?" ]; then ans=$((ans+1));
    elif [ "$ours" = "$verdict" ]; then ok=$((ok+1)); ans=$((ans+1));
    else wrong=$((wrong+1)); ans=$((ans+1)); fi
  done < <(grep '^FORMULA' "$orc" | awk '{print $2, $3}')
  fi
  complete=0; [ "$unk" = 0 ] && complete=1
  times=$(grep '^hsc-pn: answered ' "$err" | awk '{printf "%s%s:%s", (NR>1?";":""), $3, $5}')
  stats=$(grep -m1 '^hsc-pn: stats ' "$err")
  g() { echo "$stats" | grep -o "$1=[0-9.e+-]*" | cut -d= -f2; }
  flows=$(grep -m1 '^invariants: ' "$err" | awk '{print $2}'); fw=$(grep -m1 '^invariants: ' "$err" | grep -o 'widest support [0-9]*' | awk '{print $3}'); fc=$(grep -m1 '^invariants: ' "$err" | grep -o 'largest constant [0-9-]*' | awk '{print $3}')
  prot=$(grep -o '[0-9]* of [0-9]* inverted events protected' "$err" | head -1 | awk '{print $1"/"$3}')
  echo -e "$INST\t$family\t$EX\t$name\t$TAG\t$states\t${wall:-}\t$cpu\t${mem:-}\t$rc\t$status\t$ans\t$ok\t$wrong\t$unk\t$complete\t$times\t$(g reach_s)\t$(g reach_nodes)\t$(g reach_arcs)\t$(g belly_nodes)\t$(g belly_level)\t$(g belly_span)\t$(g shape_depth)\t$(g shape_units)\t$(g shape_widest)\t${flows:-}\t${fw:-}\t${fc:-}\t${prot:-}" >> "$TSV"
done
