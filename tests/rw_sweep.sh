#!/usr/bin/env bash
# rw_sweep.sh — run the BEEM corpus (or a model list) with rewrite
# directives prepended, on the symbolic engine, 15 s per model.
# Writes tests/logs/<label>_sweep.tsv: model  rc  count  nodes
# Score against a baseline archive with awk on the count/nodes columns.
# Usage: tests/rw_sweep.sh LABEL 'DIRECTIVE...' [models-file]
#   e.g. tests/rw_sweep.sh hbf '(hotbit)
# (reorder-force)' tests/logs/hb_candidates.txt
set -u
cd "$(dirname "$0")/.."
LABEL=${1:?label}
DIRECTIVES=${2:?directives}
LIST=${3:-}
OUT=tests/logs/${LABEL}_sweep.tsv
: > "$OUT"
models() {
  if [ -n "$LIST" ]; then sed 's/$/.hsc/;s|^|examples/divine/hsc/|' "$LIST"
  else ls examples/divine/hsc/*.hsc; fi
}
models | while read -r f; do
  m=$(basename "$f" .hsc)
  out=$(timeout 15 ./build/tools/hsc -e "$DIRECTIVES" "$f" \
          -e '(reach R saturate) (count R) (nodes R)' 2>&1); rc=$?
  cnt=$(sed -n 's/^R count //p' <<<"$out"); nod=$(sed -n 's/^R nodes //p' <<<"$out")
  printf '%s\t%s\t%s\t%s\n' "$m" "$rc" "${cnt:--}" "${nod:--}" >> "$OUT"
done
awk -F'\t' 'BEGIN{ok=to=0} $2==0{ok++} $2==124{to++} END{print "'"$LABEL"'": run-ok", ok, "timeout", to}' "$OUT"
