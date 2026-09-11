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
# $SWEEP_OUT/$SWEEP_TAG/rows/<instance>-<exam>.tsv (sweep_merge.sh concatenates them
# into $SWEEP_OUT/$SWEEP_TAG.tsv), logs beside as <instance>-<exam>-<heuristic>.{out,err,shape}.
# Idempotent per completed row: .done is written after the TSV, not at process start.
set -u
INST=${1:?instance}; EX=${2:?exam}; HEUR=${3:-$(dirname "$0")/heuristics.tsv}
REDUCE_ARGS=(); [ "${SWEEP_REDUCE:-0}" = 1 ] && REDUCE_ARGS=(--reduce)
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
# One rows file per (instance, exam), never a shared file: thousands of jobs
# appending to one TSV over NFS tear lines. sweep_merge.sh builds <tag>.tsv.
mkdir -p "$OUT/$TAG/rows"
TSV="$OUT/$TAG/rows/$INST-$EX.tsv"
[ -f "$OUT/$TAG/columns" ] || echo -e "instance\tfamily\texam\theuristic\ttag\tstates\twall_s\tcpu_s\tmaxrss_kb\trc\tstatus\tanswered\tok\twrong\tunknown\tcomplete\tanswer_times\treach_s\treach_nodes\treach_arcs\tbelly_nodes\tbelly_level\tbelly_span\tshape_depth\tshape_units\tshape_widest\tflows\tflows_widest\tflows_maxconst\tprotected\tshape_sig\treach_states\tpartial\treach_weighted_states\tcount_factor\tepochs" > "$OUT/$TAG/columns"
# StateSpace: `--states` instead of a property file, the four values graded
# against the oracle when it has one (an instance without an SS oracle still
# runs: its values are answered, unknown to the oracle, never wrong).
if [ "$EX" = SS ]; then QUERY="--states --cover"; [ -f "$pnml" ] || { echo -e "$INST\t$family\t$EX\t-\t$TAG\t$states\t\t\t\t\tno-input" >> "$TSV"; exit 0; }
else QUERY="--props $xml"; [ -f "$xml" ] && [ -f "$orc" ] && [ -f "$pnml" ] || { echo -e "$INST\t$family\t$EX\t-\t$TAG\t$states\t\t\t\t\tno-input" >> "$TSV"; exit 0; }; fi
declare -A seen_sig  # shape signature -> the heuristic that ran it first
grep -v '^#' "$HEUR" | while IFS=$'\t' read -r name flags envs; do
  [ -n "$name" ] || continue
  base="$OUT/$TAG/$INST-$EX-$name"; out="$base.out"; err="$base.err"; tm="$base.time"
  if [ -f "$base.done" ]; then
    if [ -f "$base.key" ]; then seen_sig[$(cat "$base.key")]=$name; fi
    continue
  fi
  printf 'prepare %s\n' "$(date +%s)" > "$base.started"
  envs_=""; [ "$envs" != "-" ] && envs_="$envs"
  # Two heuristics giving the same shape: run the first, record the second as
  # its duplicate (status dup:<name>) — the signature is cheap, no fixpoint.
  # …and the shape itself, exported beside the run (<base>.shape), for the pages and for a hand tweak.
  prep="$base.prepare"
  ( ulimit -v "$MEM"; env $envs_ timeout -k 5 120 "$BIN" -i "$pnml" $QUERY "${REDUCE_ARGS[@]}" $flags --shape-only --export-net "$prep.pnet" --export-shape "$prep.shape" -q > "$prep.out" 2> "$prep.err" ); prep_rc=$?
  sig=$(grep -m1 -o 'sig=[0-9a-f]*' "$prep.out" | cut -d= -f2)
  key=""
  # The shape hash alone omits the residual net and counting record.
  if [ "$prep_rc" = 0 ] && [ -s "$prep.shape" ]; then
    if [ "${#REDUCE_ARGS[@]}" = 0 ]; then key=$sig
    elif [ -f "$prep.pnet" ]; then key=$(cat "$prep.pnet" "$prep.shape" | sha256sum | cut -d' ' -f1); fi
  fi
  if [ -n "$key" ] && [ -n "${seen_sig[$key]:-}" ]; then
    echo -e "$INST\t$family\t$EX\t$name\t$TAG\t$states\t\t\t\t\tdup:${seen_sig[$key]}\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t$sig\t\t\t\t\t" >> "$TSV"
    : > "$out"
    cp "$prep.shape" "$base.shape"
    printf '%s\n' "$key" > "$base.key"
    rm -f "$prep.pnet"
    printf 'duplicate\n' > "$base.done.tmp"; mv "$base.done.tmp" "$base.done"
    continue
  fi
  printf 'run %s\n' "$(date +%s)" > "$base.started"
  ( ulimit -v "$MEM"; env $envs_ /usr/bin/time -f "%e %U %S %M" -o "$tm" timeout -k 5 "$BUDGET" "$BIN" -i "$pnml" $QUERY "${REDUCE_ARGS[@]}" --export-net "$base.pnet" --export-shape "$base.shape" --totalTime $((BUDGET>30 ? BUDGET-30 : 1)) --printUnknown -q -v $flags > "$out" 2> "$err" ); rc=$?
  if [ -n "$key" ] && [ -s "$base.shape" ] && cmp -s "$prep.shape" "$base.shape"; then
    if [ "${#REDUCE_ARGS[@]}" = 0 ] || cmp -s "$prep.pnet" "$base.pnet"; then
      seen_sig[$key]=$name
      printf '%s\n' "$key" > "$base.key"
    fi
  fi
  rm -f "$prep.pnet" "$base.pnet"
  read -r wall usr sys mem < <(tail -1 "$tm" 2>/dev/null)
  cpu=$(awk -v u="${usr:-0}" -v s="${sys:-0}" 'BEGIN{printf "%.2f", u+s}')
  status=ok; [ "$rc" = 124 ] && status=timeout
  grep -q "std::bad_alloc\|Cannot allocate\|out of memory" "$err" 2>/dev/null && status=memory
  [ "$rc" = 137 ] && status=killed  # SIGKILL alone does not establish an OOM
  [ "$status" = ok ] && [ "$rc" != 0 ] && status=crash
  # the budget ran out inside the reachable set: nothing was answered and no stats line came
  [ "$status" = ok ] && ! grep -q '^hsc-pn: stats ' "$err" 2>/dev/null && status=no_stats
  [ "$status" = ok ] && grep -q '^hsc-pn: stats .*partial=1' "$err" && status=partial
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
  counts=$(grep -m1 '^hsc-pn: counts ' "$err")
  stats="$stats $counts"
  g() { echo "$stats" | grep -o "$1=[0-9.e+-]*" | cut -d= -f2; }
  flows=$(grep -m1 '^invariants: ' "$err" | awk '{print $2}'); fw=$(grep -m1 '^invariants: ' "$err" | grep -o 'widest support [0-9]*' | awk '{print $3}'); fc=$(grep -m1 '^invariants: ' "$err" | grep -o 'largest constant [0-9-]*' | awk '{print $3}')
  prot=$(grep -o '[0-9]* of [0-9]* inverted events protected' "$err" | head -1 | awk '{print $1"/"$3}')
  echo -e "$INST\t$family\t$EX\t$name\t$TAG\t$states\t${wall:-}\t$cpu\t${mem:-}\t$rc\t$status\t$ans\t$ok\t$wrong\t$unk\t$complete\t$times\t$(g reach_s)\t$(g reach_nodes)\t$(g reach_arcs)\t$(g belly_nodes)\t$(g belly_level)\t$(g belly_span)\t$(g shape_depth)\t$(g shape_units)\t$(g shape_widest)\t${flows:-}\t${fw:-}\t${fc:-}\t${prot:-}\t${sig:-}\t$(g reach_states)\t$(g partial)\t$(g reach_weighted_states)\t$(g count_factor)\t$(g epochs)" >> "$TSV"
  printf '%s\n' "$rc" > "$base.done.tmp"; mv "$base.done.tmp" "$base.done"
done
printf 'complete\n' > "$OUT/$TAG/$INST-$EX.job.done"
