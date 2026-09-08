#!/bin/bash
# hsc-pn against the MCC oracle on the examples/mcc fixtures: every model with
# property files, on both input paths (PNML + MCC XML, PNET + s-expressions),
# for ReachabilityCardinality, ReachabilityFireability, UpperBounds, plus the
# deadlock and the state count of expected.csv. Each run is capped at 15 s.
#   tests/pn_samples.sh [hsc-pn binary] [examples/mcc]
#   PN_TIMEOUT=N caps each run (default 15 s); a timeout is reported, not a failure.
HSC_PN=${1:-build/tools/hsc-pn}
SAMPLES=${2:-examples/mcc}
T_=${PN_TIMEOUT:-15}
LOGS=$(dirname "$0")/logs; mkdir -p "$LOGS"
fail=0; ok=0; tmo=0
verdicts() { grep '^FORMULA' "$1" | cut -d' ' -f2,3 | sort; }
check() {  # label oracle-file output-file rc
	if [ "$4" = 124 ]; then
		# a timeout: the verdicts printed before it must still be right
		if diff <(verdicts "$2") <(verdicts "$3") | grep -q '^>'; then fail=$((fail+1)); echo "FAIL $1 (wrong verdict before timeout)"
		else tmo=$((tmo+1)); echo "TIME $1 ($(verdicts "$3" | wc -l)/$(verdicts "$2" | wc -l) answered in $T_ s)"; fi
	elif diff <(verdicts "$2") <(verdicts "$3") > "$LOGS/pn_diff.txt"; then ok=$((ok+1)); echo "ok   $1"
	else fail=$((fail+1)); echo "FAIL $1"; cat "$LOGS/pn_diff.txt"; fi
}
for pnet in "$SAMPLES"/*.pnet; do
	m=$(basename "$pnet" .pnet)
	for exam in ReachabilityCardinality:RC ReachabilityFireability:RF UpperBounds:UB; do
		e=${exam%%:*}; o=${exam##*:}
		out="$LOGS/pn_$m-$o-xml.out"
		timeout $T_ "$HSC_PN" -i "$SAMPLES/$m.pnml" --props "$SAMPLES/$m.$e.xml" -q > "$out" 2> "$out.err"; rc=$?
		check "$m $o pnml+xml" "$SAMPLES/oracle/$m-$o.out" "$out" $rc
		out="$LOGS/pn_$m-$o-sexpr.out"
		timeout $T_ "$HSC_PN" --net "$pnet" --props "$SAMPLES/$m.$e.sexpr" -q > "$out" 2> "$out.err"; rc=$?
		check "$m $o pnet+sexpr" "$SAMPLES/oracle/$m-$o.out" "$out" $rc
	done
	out="$LOGS/pn_$m-RD.out"
	timeout $T_ "$HSC_PN" -i "$SAMPLES/$m.pnml" --deadlock ReachabilityDeadlock --states -q > "$out" 2> "$out.err"; rc=$?
	check "$m RD" "$SAMPLES/oracle/$m-RD.out" "$out" $rc
	want=$(grep "^$m," "$SAMPLES/expected.csv" | cut -d, -f2)
	if grep -q "^STATE_SPACE STATES $want " "$out"; then ok=$((ok+1)); echo "ok   $m STATES $want"
	else fail=$((fail+1)); echo "FAIL $m STATES want $want got: $(grep '^STATE_SPACE' "$out" | tr '\n' ' ')"; fi
done
# TMULT: a weight of 2 on every transition doubles the arc count and leaves
# the state count alone (the block framing of INTEROP.md section 3)
MULT_NET="$LOGS/tmult.pnet"
BASE="$SAMPLES/Raft-PT-02.pnet"
T=$(timeout $T_ "$HSC_PN" --net "$BASE" --states -q 2>/dev/null | awk '/TRANSITIONS/ {print $3}')
S=$(timeout $T_ "$HSC_PN" --net "$BASE" --states -q 2>/dev/null | awk '/ STATES / {print $3}')
NT=$(python3 "$(dirname "$0")/pnet_block.py" "$BASE" "$MULT_NET" TMULT 52 --all 1 > /dev/null 2>&1 && echo ok)
if [ "$NT" = ok ] ; then
	T2=$(timeout $T_ "$HSC_PN" --net "$MULT_NET" --states -q 2>/dev/null | awk '/TRANSITIONS/ {print $3}')
	S2=$(timeout $T_ "$HSC_PN" --net "$MULT_NET" --states -q 2>/dev/null | awk '/ STATES / {print $3}')
	if [ -n "$T" ] && [ "$T2" = "$((T * 2))" ] && [ "$S2" = "$S" ] ; then
		ok=$((ok+1)); echo "ok   TMULT doubles TRANSITIONS ($T -> $T2), STATES unchanged ($S)"
	else
		fail=$((fail+1)); echo "FAIL TMULT: TRANSITIONS $T -> $T2 (want $((T * 2))), STATES $S -> $S2"
	fi
else
	fail=$((fail+1)); echo "FAIL TMULT: could not write the block"
fi

echo "$ok ok, $fail failed, $tmo timed out"
[ "$fail" = 0 ]
