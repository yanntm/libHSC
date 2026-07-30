#!/usr/bin/env bash
# CEGAR v1 sweep — produces the T-A/T-B, T-C, T-D records of the spec
# (research_notes/cegar_spec.md §10) as TSVs in this folder.
#
# Usage: ./sweep.sh BUILDDIR   (e.g. ../../build)
# Every model run is capped at 15 s; a timeout is a row, not a discard.
set -u
BUILD=${1:-../../build}
CEGAR="$BUILD/tools/cegar/hsc-cegar"
CHECK="$BUILD/tools/cegar/hsc-certcheck"
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"
TMP=$(mktemp -d "$HERE/tmp.XXXX")
trap 'rm -rf "$TMP"' EXIT

now_ms() { date +%s%3N; }

# One parity+budget row: family, params, seed; runs cegar + mono +
# certcheck, emits a T-A/T-B line.
row_ab() {
  local family=$1 params=$2 seed=$3 cts=$TMP/m.cts cert=$TMP/m.cert
  # shellcheck disable=SC2086
  "$CEGAR" gen $family $params -o "$cts" || { echo "genfail" >&2; return; }
  local run mono status=ok
  run=$(timeout 15 "$CEGAR" run "$cts" --tsv --cert "$cert") || status=timeout
  mono=$(timeout 15 "$CEGAR" mono "$cts" --cap 2000000) || status=monotimeout
  if [ "$status" != ok ]; then
    echo -e "$family\t$params\t$seed\t$status\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-"
    return
  fi
  local v_c v_m agree cc=-
  v_c=$(echo "$run" | cut -f1)
  v_m=$(echo "$mono" | cut -f1)
  agree=$([ "$v_c" = "$v_m" ] && echo yes || echo NO)
  if [ "$v_c" = holds ]; then
    if "$CHECK" "$cts" "$cert" >/dev/null 2>&1; then cc=pass; else cc=FAIL; fi
  fi
  echo -e "$family\t$params\t$seed\t$v_c\t$v_m\t$agree\t$cc\t$(echo "$run" | cut -f2-10)\t$(echo "$mono" | cut -f2)"
}

# ---------------------------------------------------------------- T-A/T-B
ab() {
  local out=ta_tb_parity.tsv
  echo -e "family\tparams\tseed\tverdict\tmono_verdict\tagree\tcertcheck\trounds\tcex\tbudget\tchaotic\tintermediate\texact\tinv\tabs_states\twitness_len\tmono_states" > "$out"
  for k in 2 3 5 8; do
    row_ab clients "$k" - >> "$out"
    row_ab clients-bug "$k" - >> "$out"
    row_ab ring "$k" - >> "$out"
  done
  for seed in $(seq 1 100); do
    row_ab rand "3 4 2 5 0.5 $seed" "$seed" >> "$out"
  done
  for seed in $(seq 1 100); do
    row_ab rand "4 5 3 8 0.4 $seed" "$seed" >> "$out"
  done
  echo "T-A/T-B done: $out"
}

# ---------------------------------------------------------------- T-C
tc() {
  local out=tc_scaling.tsv cts=$TMP/c.cts
  echo -e "family\tn\tintern\tverdict\trounds\tcex\tinv\tabs_states\tms_run\tmono_states\tms_mono" > "$out"
  for fam in clients ring; do
    for n in 2 4 8 16 32 64; do
      "$CEGAR" gen $fam "$n" -o "$cts"
      for intern in on off; do
        local flag=""
        [ $intern = off ] && flag="--no-intern"
        local t0 t1 run status=ok
        t0=$(now_ms)
        run=$(timeout 15 "$CEGAR" run "$cts" --tsv $flag) || status=timeout
        t1=$(now_ms)
        if [ $status = ok ]; then
          echo -e "$fam\t$n\t$intern\t$(echo "$run" | cut -f1-3)\t$(echo "$run" | cut -f8-9)\t$((t1-t0))\t$(mono_cells "$cts")" >> "$out"
        else
          echo -e "$fam\t$n\t$intern\ttimeout\t-\t-\t-\t-\t$((t1-t0))\t-\t-" >> "$out"
        fi
      done
    done
  done
  echo "T-C done: $out"
}
mono_cells() {
  local t0 t1 m status=ok
  t0=$(now_ms)
  m=$(timeout 15 "$CEGAR" mono "$1" --cap 5000000) || status=timeout
  t1=$(now_ms)
  if [ $status = ok ]; then
    echo -e "$(echo "$m" | cut -f2)\t$((t1-t0))"
  else
    echo -e "timeout\t$((t1-t0))"
  fi
}

# ---------------------------------------------------------------- T-D
td() {
  local out=td_policies.tsv cts=$TMP/d.cts
  echo -e "seed\tpolicy\tjump\tverdict\trounds\tcex\tabs_states\tms" > "$out"
  for seed in $(seq 1 50); do
    "$CEGAR" gen rand 3 4 2 5 0.5 "$seed" -o "$cts"
    for policy in all first cheapest; do
      for jump in off on; do
        local flag="" t0 t1 run status=ok
        [ $jump = on ] && flag="--jump-exact"
        t0=$(now_ms)
        run=$(timeout 15 "$CEGAR" run "$cts" --tsv --policy $policy $flag) || status=timeout
        t1=$(now_ms)
        if [ $status = ok ]; then
          echo -e "$seed\t$policy\t$jump\t$(echo "$run" | cut -f1-3)\t$(echo "$run" | cut -f9)\t$((t1-t0))" >> "$out"
        else
          echo -e "$seed\t$policy\t$jump\ttimeout\t-\t-\t-\t$((t1-t0))" >> "$out"
        fi
      done
    done
  done
  echo "T-D done: $out"
}

ab
tc
td
