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

### 2.4 The sample sweep: `S` alone against the state equation LP (measured)

86 models (one in five of the local corpus), RC and RF, 60 s cap per run,
`experiments/unreach/approx_sweep.sh sample`, scored against the contest
oracles (`tests/logs/unreach/sample/rows.tsv`, `summary.py`). Binary of
commit c684504 (before the abstraction pass; the zero-step verdicts are
unaffected by it).

| engine | formulas | answered | wrong | runs at the cap | total time | median |
|---|---|---|---|---|---|---|
| `hsc` — `S` from flows, zeros, safe tag | 2752 | 616 | 0 | 39 / 172 | 2708 s | 0.43 s |
| `hscu` — `S` with the unit constraints | 2752 | 643 | 0 | 40 / 172 | 2790 s | 0.49 s |
| `lp` — PetriSpot state equation | 2752 | 657 | 0 | 0 / 172 | 177 s | 0.01 s |

| examination | `hsc` | `hscu` | `lp` |
|---|---|---|---|
| RC (1376) | 391 | 400 | 504 |
| RF (1376) | 225 | 243 | 153 |

Pairwise, instances where one answered strictly more than the other:
`hscu` over `lp` 82, `lp` over `hscu` 58, `hscu` over `hsc` 18, `hsc` over
`hscu` 2.

Reading. **No wrong verdict** in 1916 answers. The two engines are
**complementary**: the LP wins on cardinality (the rational relaxation
carries the whole state equation, `x ≥ 0` included, and every place; `S`
carries the positive flows, the zeros and the tags, and answers no goal
over a removed place), `S` wins on fireability (conjunctions `p ≥ 1 ∧ q ≥ 1`
that the units and integrality refute and the rationals cannot). The unit
constraints add 27 answers, 18 on fireability. The LP costs nothing; `S`
costs 0.4 s in the median and **hits the 60 s cap on 39 of 172 runs**: on
several of them `S` was built in time (AutonomousCar-PT-03b, 3905 nodes;
BridgeAndVehicles-PT-V20P20N50, 18 713 nodes) and the time went into the
zero-step selections — cardinality atoms summing places across the shape
are non-local selectors on `S`; on others (AirplaneLD-PT-2000) nothing was
built. Mixed-sign flows, dropped today, are 312 of the 7627 flows of the
sample: not where the cardinality gap comes from.

Fixes that followed: a deadline per zero-step test (`--approx-back-time`,
default 2 s; a test cut leaves the property open and is counted), the
count of goals skipped for reading a removed place in the stats line.

### 2.5 The sample sweep with the backward search (measured)

Same 86 models, `hscb` = `hscu` + `--approx-back 50 --approx-back-time 2`
(binary of c684504; `tests/logs/unreach/sampleb/rows.tsv`):

| engine | answered / 2752 | wrong | RC / 1376 | RF / 1376 | runs at the cap | failed | total time | median |
|---|---|---|---|---|---|---|---|---|
| `hscu` | 643 | 0 | 400 | 243 | 40 | 0 | 2790 s | 0.49 s |
| `lp` | 657 | 0 | 504 | 153 | 0 | 0 | 177 s | 0.01 s |
| **`hscb`** | **1285** | **0** | **669** | **616** | 42 | 11 | 3693 s | 10.7 s |

Pairwise: `hscb` answered strictly more than `lp` on 103 instances, `lp`
more than `hscb` on 53; `hscb` over `hscu` on 83.

The backward searches: 622 decided — **592 reachable** (a layer met the
initial marking, a real path), **30 unreachable** (closed) — 6 open after
50 layers, 304 cut by their 2 s deadline. So the search inside `S` is,
first of all, a fast *reachability* engine for the targets the invariants
leave possible: the layers grow from the goal toward the initial marking
inside a set the invariants keep small, and most reachable goals are a few
layers deep. The refutations by closure are rarer (30) and the true
complement of the LP's strength; the 304 cuts are the room left.

The 11 failures were one bug: a `select` cut by the deadline before its
backward search raised through the pass (`error: deadline reached`), losing
the run's remaining formulas; fixed (the test counts as a timeout, the
property stays open). AutonomousCar-PT-03b RC after the fix: 24 s, 7 of 16
answered, 6 zero-step tests and 9 searches cut — bounded, no crash.

### 2.6 BugTracking on the abstract net: the shape strikes (measured)

The rerun of §2.3 through the abstraction pass (94 uncovered places
removed) did not return in 400 s: `S` has 1.9e14 markings there (against
8.9e129 with the capped places, whose domains were most of the count) but
**39 587 nodes against 8249** — the Sloan order was recomputed on the
abstract net and came out far worse than the original net's order, whose
dependency structure the removed places carry. Not a soundness matter, a
shape one, and the same lesson as everywhere in this project: the set's
cost is the order's. Fix: the pass now orders the abstract net by the
original net's Sloan order projected onto the kept places.

### 2.7 Two wrong verdicts, and the rule they teach (measured)

The full-corpus sweep (binary 1d53acc, the abstraction pass) produced, in
its first 178 rows, **two wrong verdicts**, both `hscb`, both *reachable*
answers from a backward search on a net with removed places
(CANConstruction-PT-005 RF-09, 24 places removed; CircularTrains-PT-024
RC-03, 17 removed). The sample sweep of §2.5, run on the capped set before
the abstraction existed, had none. The flaw is exact: a layer meeting the
initial marking is a path of the net the converses belong to — the
*abstract* net, which has more behaviour than the original, so its paths
need not exist. The two verdicts of the search are sound in complementary
settings: **reachable** on the original net (capped set or nothing
removed), **unreachable** on the abstract net (every place exact). The
pass now takes the reachable verdict only when no place was removed
(`refute_back`'s `exact_net`). What is lost with it is small: of the
sample's 592 reachable verdicts, **563 came from fully covered nets** (118
runs) and 29 from partially covered ones (36 runs); those 29 can come back
from a second search on the capped set of the original net (handoff item).

The sweep continues on the faulty binary for the LP rows, which it does not
affect; its `hscb` rows are to be redone with the fix.

### 2.8 The full corpus, first pass (partial, faulty binary)

The first full-corpus sweep (binary 1d53acc, before the reachable-verdict
fix; `tests/logs/unreach/full/rows_partial_v1.tsv`) was killed by the
system at 1203 of 1720 rows: eight `hsc-pn` runs in parallel plus the
BugTracking job overran the machine's 64 GB. The sweep script now caps
every run at 6 GB of virtual memory and runs 4 at a time. What landed:

| engine | runs | formulas | answered | wrong | at the cap | failed | median |
|---|---|---|---|---|---|---|---|
| `hscb` | 602 | 9632 | 4607 (48 %) | 4 | 60 | 20 | 7.4 s |
| `lp` | 601 | 9616 | 2374 (25 %) | 0 | 0 | 0 | 0.01 s |

RC: `hscb` 2467 / 4832, `lp` 1843 / 4816. RF: `hscb` 2140 / 4800, `lp`
531 / 4800. Pairwise `hscb` over `lp` on 383 instances, `lp` over `hscb`
on 153. The 4 wrong verdicts are all reachable answers on nets with
removed places (§2.7; RobotManipulation-PT-00005 adds two to the pair);
the 20 failures are the deadline exception of §2.5. Both fixed in the
binary of the second pass, running as this is written (`full2`).
