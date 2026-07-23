#!/bin/bash
# Compare samples/divine/status.tsv run-ok state counts against the libITS
# reference. Two oracle layers, current one first:
#   tests/logs/its_reach_counts.tsv — fresh its-reach runs on these exact
#     files (tests/collect_its_baseline.sh); authoritative where present.
#   tests/logs/beem_baseline_counts.tsv — historical counts extracted from
#     ~/git/libITS/perfs/test_*.data; some models carry several values from
#     different corpus revisions — a match against any of them counts.
#   match     : our count equals the oracle
#   MISMATCH  : oracle recorded, does not equal ours
#   no-base   : we run it, no oracle value
set -u
cd "$(dirname "$0")/.."
FRESH=tests/logs/its_reach_counts.tsv
HIST=tests/logs/beem_baseline_counts.tsv
STATUS=samples/divine/status.tsv

awk -F'\t' -v fresh="$FRESH" '
  FILENAME == fresh {
    if ($2 ~ /^[0-9]+$/) exact[$1] = $2
    next
  }
  FILENAME != fresh && FNR == NR { next }  # unreachable guard
  FILENAME ~ /beem_baseline/ { base[$1] = base[$1] " " $2; next }
  $2 == "run-ok" {
    if ($1 in exact) {
      if (exact[$1] == $3) print $1 "\tmatch\t" $3
      else print $1 "\tMISMATCH\tours=" $3 "\tits-reach=" exact[$1]
      next
    }
    if (!($1 in base)) { print $1 "\tno-base\t" $3; next }
    n = split(base[$1], b, " ")
    for (i = 1; i <= n; ++i) if (b[i] == $3) { print $1 "\tmatch\t" $3; next }
    print $1 "\tMISMATCH\tours=" $3 "\tbase=" base[$1]
  }
' "$FRESH" "$HIST" "$STATUS" | sort
