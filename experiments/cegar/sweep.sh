#!/usr/bin/env bash
# CEGAR sweep — the T-A and T-C records of the spec
# (research_notes/cegar_spec.md §10) as TSVs in this folder.
#
# Family-based: models are the examples/cegar/*_model.hsc files, sized
# by -DK/-DN. Per row, the parity triangle: the cegar verdict against
# the symbolic engine (reach + select of the bad atoms), xreach for
# the concrete count, certcheck on every holds.
#
# Usage: ./sweep.sh BUILDDIR   (e.g. ../../build)
# Every run is capped at 15 s; a timeout is a row, not a discard.
set -u
BUILD=${1:-../../build}
HERE=$(cd "$(dirname "$0")" && pwd)
HSC=$(cd "$HERE" && cd "$BUILD" && pwd)/tools/hsc
MODELS=$HERE/../../examples/cegar
cd "$HERE"
TMP=$(mktemp -d "$HERE/tmp.XXXX")
trap 'rm -rf "$TMP"' EXIT

now_ms() { date +%s%3N; }

# One row: family, model file, size flag, size, cegar options. Runs
# the cegar driver (with certificate), the symbolic+explicit parity
# driver, and certcheck on holds. Emits one TSV line.
row() {
  local family=$1 model=$2 sizeflag=$3 size=$4 opts=$5
  local drv=$TMP/drv.hsc par=$TMP/par.hsc chk=$TMP/chk.hsc
  local proof=$TMP/proof.hsc out sym status=ok
  printf '(input %s)\n(cegar v (== mon 2) %s)\n(certificate %s)\n' \
    "$MODELS/$model" "$opts" "$proof" > "$drv"
  printf '(input %s)\n(reach R)\n(count R)\n(select B R (== mon 2))\n(count B)\n(xreach x)\n' \
    "$MODELS/$model" > "$par"
  printf '(input %s)\n(certcheck %s)\n' "$MODELS/$model" "$proof" > "$chk"

  local t0 t1 wall_c wall_p
  rm -f "$proof"
  t0=$(now_ms)
  out=$(timeout 15 "$HSC" "-D$sizeflag=$size" "$drv" 2>/dev/null) || status=cegar_timeout
  t1=$(now_ms); wall_c=$((t1 - t0))
  t0=$(now_ms)
  sym=$(timeout 15 "$HSC" "-D$sizeflag=$size" "$par" 2>/dev/null) || status=${status/ok/parity_timeout}
  t1=$(now_ms); wall_p=$((t1 - t0))
  if [ "$status" != ok ]; then
    printf '%s\t%s\t%s\t%s' "$family" "$size" "$opts" "$status"
    printf '\t-%.0s' {1..15}
    printf '\n'
    return
  fi
  # v cegar VERDICT rounds R cex C/B rungs a/b/c inv I abstract S ns s/r/f
  local fields
  fields=$(echo "$out" | awk '
    $2=="cegar" { split($7, cb, "/");
      v=$3; ro=$5; c=cb[1]; b=cb[2]; ru=$9; iv=$11; ab=$13; ns=$15; wl="-" }
    $2=="witness" { wl=NF-2 }
    END { print v, ro, c, b, ru, iv, ab, ns, wl }')
  local verdict rounds cex budget rungs inv abs ns wl
  read -r verdict rounds cex budget rungs inv abs ns wl <<< "$fields"
  local sym_states sym_bad x_states
  sym_states=$(echo "$sym" | awk '$1=="R" && $2=="count" {print $3}')
  sym_bad=$(echo "$sym" | awk '$1=="B" && $2=="count" {print $3}')
  x_states=$(echo "$sym" | awk '$2=="xreach" {print $3}')
  local sym_verdict=violation agree cc=-
  [ "$sym_bad" = 0 ] && sym_verdict=holds
  agree=$([ "$verdict" = "$sym_verdict" ] && echo yes || echo NO)
  if [ "$verdict" = holds ]; then
    if timeout 15 "$HSC" "-D$sizeflag=$size" "$chk" > /dev/null 2>&1; then
      cc=pass
    else
      cc=FAIL
    fi
  fi
  printf '%s\t%s\t%s\tok\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$family" "$size" "$opts" "$verdict" "$sym_verdict" "$agree" "$cc" \
    "$rounds" "$cex" "$budget" "$rungs" "$inv" "$abs" "$wl" \
    "$sym_states" "$x_states" "$ns" "$wall_c/$wall_p"
}

HEADER='family\tsize\topts\tstatus\tverdict\tsym_verdict\tagree\tcertcheck\trounds\tcex\tbudget\trungs_c/i/e\tinv\tabs_states\twitness_len\tsym_states\txreach_states\tns_search/replay/refine\twall_ms_cegar/parity'

# ---------------------------------------------------------------- T-A
ta() {
  local out=ta_parity.tsv
  printf "$HEADER\n" > "$out"
  for k in 2 3 5 8; do
    row clients     clients_model.hsc     K "$k" all >> "$out"
    row clients-bug clients_bug_model.hsc K "$k" all >> "$out"
    row ring        ring_model.hsc        N "$k" all >> "$out"
  done
  echo "T-A -> $out"
}

# ---------------------------------------------------------------- T-C
tc() {
  local out=tc_scaling.tsv
  printf "$HEADER\n" > "$out"
  for n in 2 4 8 16 32 64; do
    row clients clients_model.hsc K "$n" all >> "$out"
    row clients clients_model.hsc K "$n" "all no-intern" >> "$out"
    row ring    ring_model.hsc    N "$n" all >> "$out"
    row ring    ring_model.hsc    N "$n" "all no-intern" >> "$out"
  done
  echo "T-C -> $out"
}

ta
tc
