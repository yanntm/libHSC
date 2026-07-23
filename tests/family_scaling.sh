#!/usr/bin/env bash
# One build+reach of the balanced parametric philosophers at N = 2^K,
# through the declared family route by default.
#
#   usage: family_scaling.sh K [declared|unfold|check]
#
# Prints one line: "k=K N=2^K <bill> wall=SECONDS". A sweep is one call
# per K (the per-example diagnostic bound applies to each call, not the
# sweep). The op-term count in the bill is the M4 gate-2 measurement:
# linear in K on the declared route, ~6N on the unfold route.
set -eu
cd "$(dirname "$0")/.."
K="${1:?usage: family_scaling.sh K [declared|unfold|check]}"
MODE="${2:-declared}"
N=$((1 << K))
START=$(date +%s.%N)
BILL=$(HSC_FAMILY="$MODE" ./build/examples/hscrun -DN="$N" \
  samples/param/layout/philo_balanced.hsc | grep '^bill:')
END=$(date +%s.%N)
echo "k=$K N=$N $BILL wall=$(echo "$END $START" | awk '{printf "%.2f", $1-$2}')"
