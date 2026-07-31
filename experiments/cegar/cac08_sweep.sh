#!/usr/bin/env bash
# CAC08 sweep — the translated corpus (examples/cac08/hsc/, one row per
# subject) through the parity triangle: cegar verdict, symbolic engine
# (reach + select of the bad atom), explicit count (xreach), certcheck
# on every holds. The bad atom (== mon E) is read from each subject's
# generated driver — E varies per property.
#
# Usage: ./cac08_sweep.sh BUILDDIR [BUDGET_S]   (e.g. ../../build 480)
# Each of the four phases (cegar / sym / xpl / certcheck) is capped at
# 15 s independently, so one slow engine never starves another's column;
# a timeout is named in the status, and the finished phases still report.
# Rows stream: each subject's TSV row prints to stdout the moment it is
# done, and is appended to the TSV at the same time. The sweep exits on
# its own once the time budget is spent (default 480 s) — run it again
# to continue: completed subjects are skipped. It never needs killing.
set -u
BUILD=${1:-../../build}
BUDGET_S=${2:-480}
HERE=$(cd "$(dirname "$0")" && pwd)
HSC=$(cd "$HERE" && cd "$BUILD" && pwd)/tools/hsc
CORPUS=$(cd "$HERE/../../examples/cac08/hsc" && pwd)
cd "$HERE"
TMP=$(mktemp -d "$HERE/tmp.XXXX")
trap 'rm -rf "$TMP"' EXIT
T_START=$(date +%s)

now_ms() { date +%s%3N; }

row() { # row SYSTEM SUBJECT MODELPATH DRIVERPATH
  local system=$1 subject=$2 model=$3 driver=$4
  local E
  E=$(sed -n 's/.*(== mon \([0-9]*\)).*/\1/p' "$driver")
  local drv=$TMP/drv.hsc sym=$TMP/sym.hsc xpl=$TMP/xpl.hsc chk=$TMP/chk.hsc
  local proof=$TMP/proof.hsc c_out s_out x_out
  printf '(input %s)\n(cegar v (== mon %s))\n(certificate %s)\n' \
    "$model" "$E" "$proof" > "$drv"
  printf '(input %s)\n(reach R)\n(count R)\n(select B R (== mon %s))\n(count B)\n' \
    "$model" "$E" > "$sym"
  printf '(input %s)\n(xreach x)\n' "$model" > "$xpl"
  printf '(input %s)\n(certcheck %s)\n' "$model" "$proof" > "$chk"

  local t0 late="" wall_c wall_s wall_x
  rm -f "$proof"
  t0=$(now_ms)
  c_out=$(timeout 15 "$HSC" "$drv" 2>/dev/null) || late="cegar"
  wall_c=$(( $(now_ms) - t0 ))
  t0=$(now_ms)
  s_out=$(timeout 15 "$HSC" "$sym" 2>/dev/null) || late="$late sym"
  wall_s=$(( $(now_ms) - t0 ))
  t0=$(now_ms)
  x_out=$(timeout 15 "$HSC" "$xpl" 2>/dev/null) || late="$late xpl"
  wall_x=$(( $(now_ms) - t0 ))

  local status=ok
  [ -n "$late" ] && status="timeout:$(echo $late | tr ' ' '+')"

  local verdict=- rounds=- cex=- budget=- rungs=- inv=- abs=- ns=- wl=-
  case "$late" in *cegar*) ;; *)
    local fields
    fields=$(echo "$c_out" | awk '
      $2=="cegar" { split($7, cb, "/");
        v=$3; ro=$5; c=cb[1]; b=cb[2]; ru=$9; iv=$11; ab=$13; ns=$15; wl="-" }
      $2=="witness" { wl=NF-2 }
      END { print v, ro, c, b, ru, iv, ab, ns, wl }')
    read -r verdict rounds cex budget rungs inv abs ns wl <<< "$fields"
  ;; esac
  local sym_states=- sym_bad=- sym_verdict=- agree=-
  case "$late" in *sym*) ;; *)
    sym_states=$(echo "$s_out" | awk '$1=="R" && $2=="count" {print $3}')
    sym_bad=$(echo "$s_out" | awk '$1=="B" && $2=="count" {print $3}')
    sym_verdict=violation
    [ "$sym_bad" = 0 ] && sym_verdict=holds
    [ "$verdict" != - ] &&
      agree=$([ "$verdict" = "$sym_verdict" ] && echo yes || echo NO)
  ;; esac
  local x_states=-
  case "$late" in *xpl*) ;; *)
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

OUT=cac08_verdicts.tsv
[ -f "$OUT" ] || printf 'system\tsubject\tstatus\tverdict\tsym_verdict\tagree\tcertcheck\trounds\tcex\tbudget\trungs_c/i/e\tinv\tabs_states\twitness_len\tsym_states\txreach_states\tns_search/replay/refine\twall_ms_cegar/sym/xpl\n' > "$OUT"
for model in "$CORPUS"/*/*_model.hsc; do
  system=$(basename "$(dirname "$model")")
  subject=$(basename "$model" _model.hsc)
  driver=${model%_model.hsc}.hsc
  if awk -F'\t' -v s="$system" -v j="$subject" '$1==s && $2==j {found=1} END {exit !found}' "$OUT"; then
    continue
  fi
  if [ $(( $(date +%s) - T_START )) -ge "$BUDGET_S" ]; then
    echo "budget spent ($BUDGET_S s) — rerun to continue from $subject"
    exit 0
  fi
  line=$(row "$system" "$subject" "$model" "$driver")
  printf '%s\n' "$line" >> "$OUT"
  printf '%s\n' "$line"
done
echo "CAC08 complete -> $OUT"
