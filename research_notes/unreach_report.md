# Unreachability from an over-approximation — engineering report

The log of the autonomous session on the unreachability engine: a diagram
`S ⊇ R` built from linear facts (flows, structural zeros, NUPN units),
symbolic steps forward or backward on it, fixpoints thereof, with one goal:
prove impossibilities — a formula no reachable marking satisfies, a
transition no reachable marking fires — without the reachable set. The
dead-transition test (`include/hsc/linear/algorithm.md`) is the first
instance; the reachability examinations of the MCC are the second.

Design lives in `include/hsc/linear/algorithm.md`; the ideas ledger in
`research_notes/ideas.md` (#1, #17–#20). This file is the record of what
was tried, measured, and concluded, newest section last. Numbers here are
traceable to the logs named beside them (`tests/logs/unreach/`,
`experiments/unreach/`).

## 0. Starting point

* `S` from the positive P-flows (bounds and equalities), the structural
  zeros (never-marked places), the box; `(full)`, `(equality)`,
  `(intersect)`, `(dead)` in the surface; `hsc-pn --dead S [--dead-step]`.
* BugTracking-PT-q3m016: 24 601 of 27 370 transitions dead in 15 s, all
  oracle-confirmed; the forward one-step image of `S` does not return in
  385 s (under all events or the 2769 live ones). The backward reading of
  the test is designed, not built.
* Corpus at hand: 430 P/T instances under `tests/logs/mcc2026/INPUTS/`
  with their property files; oracles for QuasiLiveness, StableMarking and
  UpperBounds under `/data/ythierry/MCC26logs/_shared/oracles-merged-2026/`.

## 1. Plan

1. **Reachability formulas on `S`** (ledger #19): `hsc-pn --approx` builds
   `S` before `R`; a reachability target disjoint from `S` is `FALSE`, an
   invariant whose negation misses `S` is `TRUE`; anything else stays
   open for `R`. Measure on the local corpus: formulas refuted by `S`
   alone, per examination; cross-check against `R` where `R` completes
   (an exact oracle) and against the contest oracles where they exist.
2. **Refining `S` from the NUPN tags**: a safe net bounds every place by 1
   (the box), and each unit holds at most one token (`Σ_{p ∈ u} p ≤ 1`, an
   inequality diagram — the knapsack with the residuals ≤ K kept). Does
   it change the size of `S` and the count of refuted formulas?
3. **One backward step within `S`**: `init ∩ X = ∅ ∧ pre(X) ∩ (S ∖ X) = ∅`
   ⇒ `X` unreachable, converses restricted to `S` (the leaf domains must
   be as wide as the box), live events only. Then the fixpoint
   `pre*(X) ∩ S`. Measure what each step adds over step 1.
4. **The dead-transition test backward, per slice** (BugTracking's 2769
   survivors against ITS-Tools' 1098 SMT kills), if time allows.

Rules: local runs only, bounded per instance, logs under
`tests/logs/unreach/`; commits as increments land.

## 2. Log

### 2.1 `--approx`: reachability formulas refuted on `S` (built)

`hsc-pn --approx S [--approx-only] [--approx-units]` (`tools/pn_approx.hh`,
`solver::refute`), `(at-most …)` for the unit constraints. First look,
AirplaneLD-PT-0010 (89 places, safe NUPN), against the contest oracles
(`/data/ythierry/MCC26deploy/MCC-drivers/oracle`), PetriSpot's state
equation LP (`petri64 --lp`, rational relaxation, 5 s per property) beside:

| engine | RC answered / 16 | RF answered / 16 | wrong | time |
|---|---|---|---|---|
| `S` (flows, zeros, safe tag) | 13 | 4 | 0 | < 0.1 s |
| `S` + unit constraints | 13 | 5 | 0 | < 0.1 s |
| PetriSpot `--lp` | 11 | 2 | 0 | 0.03 s |

`S` has 1.27e15 markings without the unit constraints and 142 272 with them
(the reachable set is far smaller still). Logs: `tests/logs/unreach/air_*`,
`lp_airplane_*`. BugTracking's dead count is unchanged through the shared
builder (24 601, 15.4 s).

Next: the sample sweep (`experiments/unreach/approx_sweep.sh`, 86 models,
RC and RF, three engines, 60 s cap each) for the corpus-wide picture and
any wrong verdict.
