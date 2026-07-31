#!/usr/bin/env bash
# CAC08 sweep — the translated corpus (examples/cac08/hsc/, one row per
# subject) through the parity triangle: cegar verdict, symbolic engine
# (reach + select of the bad atom), explicit count (xreach), certcheck
# on every holds. The bad atom (== mon E) is read from each subject's
# generated driver — E varies per property.
#
# Usage: ./cac08_sweep.sh BUILDDIR   (e.g. ../../build)
# The only timeouts are per hsc invocation: each of the four phases
# (cegar / sym / xpl / certcheck) is capped at 15 s independently, so
# one slow engine never starves another's column. Rows stream: each
# subject's TSV row prints to stdout the moment it is done, and is
# appended to the TSV at the same time. Subjects already in the TSV are
# skipped, so an interrupted sweep continues where it left off.
# Variant knobs: CORPUS_DIR (default examples/cac08/hsc), OUT_TSV
# (default cac08_verdicts.tsv), CHAIN — rewrite directives inserted into
# every probe after its (input …) line, e.g. '(hotbit 17 100000)'.
set -u
BUILD=${1:-../../build}
HERE=$(cd "$(dirname "$0")" && pwd)
HSC=$(cd "$HERE" && cd "$BUILD" && pwd)/tools/hsc
CORPUS=$(cd "$HERE/../../examples/cac08/${CORPUS_DIR:-hsc}" && pwd)
CHAIN=${CHAIN:-}
cd "$HERE"
TMP=$(mktemp -d "$HERE/tmp.XXXX")
trap 'rm -rf "$TMP"' EXIT

now_ms() { date +%s%3N; }

row() { # row SYSTEM SUBJECT MODELPATH DRIVERPATH
  local system=$1 subject=$2 model=$3 driver=$4
  local E
  E=$(sed -n 's/.*(== mon \([0-9]*\)).*/\1/p' "$driver")
  local drv=$TMP/drv.hsc sym=$TMP/sym.hsc xpl=$TMP/xpl.hsc chk=$TMP/chk.hsc
  local proof=$TMP/proof.hsc c_out s_out x_out
  printf '(input %s)\n%s\n(cegar v (== mon %s))\n(certificate %s)\n' \
    "$model" "$CHAIN" "$E" "$proof" > "$drv"
  printf '(input %s)\n%s\n(reach R)\n(count R)\n(select B R (== mon %s))\n(count B)\n' \
    "$model" "$CHAIN" "$E" > "$sym"
  printf '(input %s)\n%s\n(xreach x)\n' "$model" "$CHAIN" > "$xpl"
  printf '(input %s)\n%s\n(certcheck %s)\n' "$model" "$CHAIN" "$proof" > "$chk"

  # Per phase: ok, timeout (exit 124 from timeout(1)), or fail (any other
  # nonzero — a refusal or crash, reported as such, never as a timeout).
  local t0 rc wall_c wall_s wall_x c_st=ok s_st=ok x_st=ok
  rm -f "$proof"
  t0=$(now_ms)
  c_out=$(timeout 15 "$HSC" "$drv" 2>/dev/null); rc=$?
  [ $rc -ne 0 ] && { c_st=fail; [ $rc -eq 124 ] && c_st=timeout; }
  wall_c=$(( $(now_ms) - t0 ))
  t0=$(now_ms)
  s_out=$(timeout 15 "$HSC" "$sym" 2>/dev/null); rc=$?
  [ $rc -ne 0 ] && { s_st=fail; [ $rc -eq 124 ] && s_st=timeout; }
  wall_s=$(( $(now_ms) - t0 ))
  t0=$(now_ms)
  x_out=$(timeout 15 "$HSC" "$xpl" 2>/dev/null); rc=$?
  [ $rc -ne 0 ] && { x_st=fail; [ $rc -eq 124 ] && x_st=timeout; }
  wall_x=$(( $(now_ms) - t0 ))

  local status=ok bad=""
  [ $c_st != ok ] && bad="cegar_$c_st"
  [ $s_st != ok ] && bad="$bad sym_$s_st"
  [ $x_st != ok ] && bad="$bad xpl_$x_st"
  [ -n "$bad" ] && status=$(echo $bad | tr ' ' '+')

  local verdict=- rounds=- cex=- budget=- rungs=- inv=- abs=- ns=- wl=-
  case "$c_st" in timeout|fail) ;; *)
    local fields
    fields=$(echo "$c_out" | awk '
      $2=="cegar" { split($7, cb, "/");
        v=$3; ro=$5; c=cb[1]; b=cb[2]; ru=$9; iv=$11; ab=$13; ns=$15; wl="-" }
      $2=="witness" { wl=NF-2 }
      END { print v, ro, c, b, ru, iv, ab, ns, wl }')
    read -r verdict rounds cex budget rungs inv abs ns wl <<< "$fields"
  ;; esac
  local sym_states=- sym_bad=- sym_verdict=- agree=-
  case "$s_st" in timeout|fail) ;; *)
    sym_states=$(echo "$s_out" | awk '$1=="R" && $2=="count" {print $3}')
    sym_bad=$(echo "$s_out" | awk '$1=="B" && $2=="count" {print $3}')
    sym_verdict=violation
    [ "$sym_bad" = 0 ] && sym_verdict=holds
    [ "$verdict" != - ] &&
      agree=$([ "$verdict" = "$sym_verdict" ] && echo yes || echo NO)
  ;; esac
  local x_states=-
  case "$x_st" in timeout|fail) ;; *)
    x_states=$(echo "$x_out" | awk '$2=="xreach" {print $3}')
  ;; esac
  local cc=-
  if [ "$verdict" = holds ]; then
    if timeout 15 "$HSC" "$chk" > /dev/null 2>&1; then cc=pass; else cc=FAIL; fi
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$system" "$subject" "$status" "$verdict" "$sym_verdict" "$agree" "$cc" \
    "$rounds" "$cex" "$budget" "$rungs" "$inv" "$abs" "$wl" \
    "$sym_states" "$x_states" "$ns" "$wall_c/$wall_s/$wall_x"
}

OUT=${OUT_TSV:-cac08_verdicts.tsv}
[ -f "$OUT" ] || printf 'system\tsubject\tstatus\tverdict\tsym_verdict\tagree\tcertcheck\trounds\tcex\tbudget\trungs_c/i/e\tinv\tabs_states\twitness_len\tsym_states\txreach_states\tns_search/replay/refine\twall_ms_cegar/sym/xpl\n' > "$OUT"
for model in "$CORPUS"/*/*_model.hsc; do
  system=$(basename "$(dirname "$model")")
  subject=$(basename "$model" _model.hsc)
  driver=${model%_model.hsc}.hsc
  if awk -F'\t' -v s="$system" -v j="$subject" '$1==s && $2==j {found=1} END {exit !found}' "$OUT"; then
    continue
  fi
  line=$(row "$system" "$subject" "$model" "$driver")
  printf '%s\n' "$line" >> "$OUT"
  printf '%s\n' "$line"
done
echo "CAC08 complete -> $OUT"
