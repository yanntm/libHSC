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

### 2.2 The backward search inside `S` (built)

`(pre …)`, `(minus …)`, `(backward NAME X SET [steps K] [writing LEAF*])`
in the surface (`src/surface_backward.cc`): the converses of the default
system inverted against `S` (kept per set, no protection — every
predecessor is met with `S`), layers from the goal's markings of `S`; the
first layer restricted to the events writing a place the goal reads. A
layer meeting the initial marking is a real path (reachable); a closed
search is unreachable when every place is exact in `S`. Client:
`hsc-pn --approx-back K --approx-back-time T`. The leaf domains are widened
to the box before the model is emitted (`approx_facts` before
`to_surface`), since the converses are restricted to the declared domains.

AirplaneLD-PT-0010, `--approx-units --approx-back 50`, 2 s per property:

| examination | `S` alone | + backward | wrong | layers to the initial marking | time |
|---|---|---|---|---|---|
| RC | 13 / 16 | 15 / 16 (2 reachable) | 0 | 6, 6 | 0.10 s |
| RF | 5 / 16 | 16 / 16 (11 reachable) | 0 | 3–9 | 0.14 s |

Every open formula of Airplane was reachable, and the backward search
found the path in a few layers of a 142 272-marking set. The other verdict
(closed) did not occur here. Logs `tests/logs/unreach/airb_*`.

### 2.3 BugTracking: the one-step test backward, per slice (measured)

`hsc-pn --dead 20 --dead-step` with the test rewritten backward
(`pre(en(t)) ∩ (S ∖ en(t))` under the live events writing a leaf the guard
reads, iterated): **24 771 of 27 370 dead** — 24 601 by the structural
zeros, **170 by the one-step test** — in 188 s (tests 179 s, about 65 ms
per candidate slice over 2769 candidates). Against the QLA oracle: the 170
are all among its 2523 unknowns, 0 contradicted. ITS-Tools' SMT kills 1098
of those unknowns in 61 s; 170 in 179 s is the first backward cut, with
the whole per-slice cost still on the table (the slices are not thin in
nodes). Log: `tests/logs/unreach/dead_bt_back.err`.

Caveat, and the fix that followed: this run took `S` with the 94 uncovered
places *capped*, so a real predecessor beyond a cap could be missed and
the one-step verdicts were not guaranteed sound (the oracle did not catch
one, but that is not a proof). The pass now runs on the **abstract net**
— the uncovered places removed with their arcs (`tools/pn_abstract.hh`),
in a session of its own (`tools/pn_approx_pass.hh`) — where every place is
exact and every verdict is sound for the original net on the places kept.
The rerun on the abstract net is next.
