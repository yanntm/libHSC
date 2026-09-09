#!/usr/bin/env bash
# run_ctl_bench.sh: hsc-pn on the CTLCardinality / CTLFireability examinations of
# a list of MCC 2026 P/T instances, against the contest oracle, locally.
#   experiments/ctl/run_ctl_bench.sh [-t cap_s] [-j jobs] [-b hsc-pn] [-m mem_kb] [-e "extra flags"] -o results.tsv models.txt
# models.txt: "<instance> <states>" per line. Inputs come from
# ~/git/pnmcc-models-2026/website (INPUTS/<instance>.tgz, oracle.tar.gz), extracted
# once under tests/logs/mcc2026/. One process per (instance, examination), each
# under `timeout` and `ulimit -v`; the tool's own --totalTime is the cap minus one
# second so it prints UNKNOWN for what it left open. Output: one TSV line per run
#   instance exam states wall_s maxrss_kb answered ok wrong unknown protected note
# where `protected` is the number of inverted events hsc-pn intersected with R
# (from its -v report; empty when the run never inverted), `note` the exit status.
set -u
CAP=60; JOBS=8; BIN=build/tools/hsc-pn; MEM=6000000; EXTRA=""; OUT=""
while getopts "t:j:b:m:e:o:" opt; do case $opt in
  t) CAP=$OPTARG;; j) JOBS=$OPTARG;; b) BIN=$OPTARG;; m) MEM=$OPTARG;; e) EXTRA=$OPTARG;; o) OUT=$OPTARG;; esac; done
shift $((OPTIND-1))
LIST=${1:?models.txt}
[ -n "$OUT" ] || { echo "-o results.tsv required"; exit 2; }
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
W=$ROOT/tests/logs/mcc2026; mkdir -p "$W/INPUTS" "$W/oracle" "$W/runs"
CORPUS=$HOME/git/pnmcc-models-2026/website
[ -d "$W/oracle/oracle" ] || tar xzf "$CORPUS/oracle.tar.gz" -C "$W/oracle"
export CAP BIN MEM EXTRA W ROOT
one() {  # instance states exam short
  local m=$1 st=$2 x=$3 o=$4
  [ -d "$W/INPUTS/$m" ] || tar xzf "$CORPUS_/$m.tgz" -C "$W/INPUTS" 2>/dev/null
  local xml="$W/INPUTS/$m/$x.xml" orc="$W/oracle/oracle/$m-$o.out"
  [ -f "$xml" ] && [ -f "$orc" ] || { echo -e "$m\t$o\t$st\t\t\t\t\t\t\t\tno-input"; return; }
  local out="$W/runs/$m-$o.out" err="$W/runs/$m-$o.err" tm="$W/runs/$m-$o.time"
  ( ulimit -v $MEM; /usr/bin/time -f "%e %M" -o "$tm" timeout $CAP "$ROOT/$BIN" -i "$W/INPUTS/$m/model.pnml" --props "$xml" --totalTime $((CAP-1)) --printUnknown -q -v $EXTRA > "$out" 2> "$err" ); local rc=$?
  local wall mem; read -r wall mem < <(tail -1 "$tm" 2>/dev/null)
  local ok=0 wrong=0 unk=0 ans=0
  while read -r name verdict; do
    local ours; ours=$(grep -E "^FORMULA $name " "$out" | head -1 | awk '{print $3}')
    if [ -z "$ours" ]; then unk=$((unk+1));
    elif [ "$verdict" = "?" ]; then ans=$((ans+1));
    elif [ "$ours" = "$verdict" ]; then ok=$((ok+1)); ans=$((ans+1));
    else wrong=$((wrong+1)); ans=$((ans+1)); fi
  done < <(grep '^FORMULA' "$orc" | awk '{print $2, $3}')
  local prot; prot=$(grep -o '[0-9]* of [0-9]* inverted events protected' "$err" | head -1 | awk '{print $1"/"$3}')
  echo -e "$m\t$o\t$st\t$wall\t$mem\t$ans\t$ok\t$wrong\t$unk\t$prot\trc=$rc"
}
export -f one; export CORPUS_="$CORPUS/INPUTS"
{ echo -e "instance\texam\tstates\twall_s\tmaxrss_kb\tanswered\tok\twrong\tunknown\tprotected\tnote";
  while read -r m st; do echo "$m $st CTLCardinality CTLC"; echo "$m $st CTLFireability CTLF"; done < "$LIST" \
  | xargs -P "$JOBS" -L 1 bash -c 'one "$@"' _ ; } > "$OUT"
awk -F'\t' 'NR>1{a+=$6; o+=$7; w+=$8; u+=$9; n++} END{printf "%d runs, answered %d, ok %d, wrong %d, unknown %d\n", n, a, o, w, u}' "$OUT"
