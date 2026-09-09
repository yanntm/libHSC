#!/usr/bin/env bash
# sweep_local.sh: the sweep on this machine — every (instance, exam) of a list
# through sweep_job.sh, N side by side (SWEEP.md). Inputs come from the local
# MCC 2026 copy (tests/logs/mcc2026, as the CTL bench lays it out).
#   experiments/order/sweep_local.sh [-j N] [-t budget_s] [-x "CTLC CTLF"] [-h heuristics.tsv] -o TAG models.txt
# models.txt: "<instance> <states>" per line (experiments/ctl/models_*.txt).
set -u
JOBS=4; BUDGET=300; EXAMS="CTLC CTLF"; HEUR=$(dirname "$0")/heuristics.tsv; TAG=""
while getopts "j:t:x:h:o:" opt; do case $opt in
  j) JOBS=$OPTARG;; t) BUDGET=$OPTARG;; x) EXAMS=$OPTARG;; h) HEUR=$OPTARG;; o) TAG=$OPTARG;; esac; done
shift $((OPTIND-1)); LIST=${1:?models.txt}; [ -n "$TAG" ] || { echo "-o TAG required"; exit 2; }
ROOT=$(cd "$(dirname "$0")/../.." && pwd); W=$ROOT/tests/logs/mcc2026
CORPUS=$HOME/git/pnmcc-models-2026/website
[ -d "$W/oracle/oracle" ] || { mkdir -p "$W/oracle"; tar xzf "$CORPUS/oracle.tar.gz" -C "$W/oracle"; }
export SWEEP_BUDGET=$BUDGET SWEEP_INPUTS=$W/INPUTS SWEEP_ORACLE=$W/oracle/oracle SWEEP_OUT=$ROOT/experiments/order/results SWEEP_TAG=$TAG
export SWEEP_BIN=${SWEEP_BIN:-$ROOT/build/tools/hsc-pn} HEUR W CORPUS
mkdir -p "$W/INPUTS" "$SWEEP_OUT"
one() { local m=$1 x=$2
  [ -d "$W/INPUTS/$m" ] || tar xzf "$CORPUS/INPUTS/$m.tgz" -C "$W/INPUTS" 2>/dev/null
  "$(dirname "$0")/sweep_job.sh" "$m" "$x" "$HEUR"; }
export -f one
awk '!/^#/ && NF{print $1}' "$LIST" | while read -r m; do for x in $EXAMS; do echo "$m $x"; done; done \
  | xargs -P "$JOBS" -n 2 bash -c 'one "$0" "$1"'
echo "sweep $TAG: $(($(wc -l < "$SWEEP_OUT/$TAG.tsv") - 1)) runs in $SWEEP_OUT/$TAG.tsv"
