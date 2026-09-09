#!/usr/bin/env bash
# sweep_oar.sh: submit the sweep to the cluster — one OAR job per (instance,
# exam), the job running every heuristic in sequence through sweep_job.sh
# (SWEEP.md §4–§5). Run on the head node from the deployed folder, which holds
# hsc-pn, heuristics.tsv, sweep_job.sh and this script; the harness tree
# (~/MCC26/MCC-drivers) provides INPUTS/<instance>/ and oracle/.
#   ./sweep_oar.sh [-x "CTLC CTLF"] [-t budget_s] [-w walltime] [-H hosts] -o TAG models.txt
# Never rewrite a script while a submission loop reads it (CLUSTER.md).
set -u
EXAMS="CTLC CTLF"; BUDGET=300; WALL=""; HOSTS="tall%"; TAG=""
while getopts "x:t:w:H:o:" opt; do case $opt in
  x) EXAMS=$OPTARG;; t) BUDGET=$OPTARG;; w) WALL=$OPTARG;; H) HOSTS=$OPTARG;; o) TAG=$OPTARG;; esac; done
shift $((OPTIND-1)); LIST=${1:?models.txt}; [ -n "$TAG" ] || { echo "-o TAG required"; exit 2; }
HERE=$(cd "$(dirname "$0")" && pwd); HARNESS=${HARNESS:-$HOME/MCC26/MCC-drivers}
NH=$(grep -vc '^#' "$HERE/heuristics.tsv")
# walltime: the heuristics in sequence at the budget, plus a tenth for the setup
[ -n "$WALL" ] || WALL=$(( (NH * BUDGET * 11 / 10 + 59) / 60 )); [[ $WALL == *:* ]] || WALL="0:$WALL:0"
mkdir -p "$HERE/results/$TAG"; cd "$HERE/results/$TAG"
n=0
awk '!/^#/ && NF{print $1}' "$LIST" | while read -r m; do for x in $EXAMS; do
  oarsub -l "/nodes=1/core=1,walltime=$WALL" -p "(host like '$HOSTS')" \
    "cd $HERE && SWEEP_BUDGET=$BUDGET SWEEP_BIN=$HERE/hsc-pn SWEEP_INPUTS=$HARNESS/INPUTS SWEEP_ORACLE=$HARNESS/oracle SWEEP_OUT=$HERE/results SWEEP_TAG=$TAG ./sweep_job.sh $m $x ; exit" > /dev/null
  n=$((n+1))
done; done
echo "submitted $n jobs: $NH heuristics x ${BUDGET}s each, walltime $WALL, hosts $HOSTS, results in $HERE/results/$TAG.tsv"
