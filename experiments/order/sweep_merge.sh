#!/usr/bin/env bash
# sweep_merge.sh: build <tag>.tsv from a sweep's rows files (one per instance and
# examination, written by sweep_job.sh), plus whatever an older shared TSV
# holds; a (instance, exam, heuristic) present in a rows file wins over the
# shared file's line; torn lines (a field count outside 30..33) are dropped.
#   experiments/order/sweep_merge.sh RESULTS_DIR TAG      -> RESULTS_DIR/TAG.tsv
set -eu
DIR=${1:?results dir}; TAG=${2:?tag}; D="$DIR/$TAG"
COLS="$D/columns"; [ -f "$COLS" ] || COLS=""
{ if [ -n "$COLS" ]; then cat "$COLS"; else head -1 "$DIR/$TAG.tsv"; fi
  { [ -d "$D/rows" ] && cat "$D/rows"/*.tsv 2>/dev/null; [ -f "$DIR/$TAG.tsv" ] && tail -n +2 "$DIR/$TAG.tsv"; } \
  | awk -F'\t' '(NF>=30&&NF<=33) && $1!="" && !seen[$1 FS $3 FS $4]++'; } > "$DIR/$TAG.merged.tsv"
mv "$DIR/$TAG.merged.tsv" "$DIR/$TAG.tsv"
echo "$DIR/$TAG.tsv: $(($(wc -l < "$DIR/$TAG.tsv") - 1)) rows"
