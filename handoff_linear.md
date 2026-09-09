# Handoff — the unreachability engine (`include/hsc/linear/`, `tools/pn_approx*.hh`)

Current state only, next action first. Design: `include/hsc/linear/algorithm.md`.
Report: `research_notes/unreach_report.md`. Ledger: `research_notes/ideas.md`
#1, #17–#22. Campaign: `experiments/unreach/`.

## What stands

`hsc-pn --approx S [--approx-units] [--approx-back K] [--approx-back-time T]
[--approx-only]` runs a pass in its own session: the net abstracted to the
places the linear facts bound (uncovered places removed with their arcs,
`pn_abstract.hh`), ordered by the original net's Sloan order projected,
leaf domains as wide as the box; the invariant set `S` from every flow
(signed knapsack), the structural zeros, the NUPN safe tag and unit
constraints; then the zero-step refutations (`(select … S …)` empty), then
a backward search inside `S` per open property (`(backward …)`: a layer
meeting the initial marking is a real path, a closed search is
unreachable). `--dead S [--dead-step]` is the same pass for dead
transitions, the one-step test backward per slice. Every verdict is sound
for the original net on the places kept; a goal reading a removed place is
not asked.

Measured on the sample (86 models, RC+RF, 2752 formulas, 60 s cap): `S`
alone 643 answers, PetriSpot's LP 657, `S` + backward **1285**, 0 wrong
anywhere. The two over-approximations are complementary (LP: cardinality,
`x ≥ 0`, every place; `S`: fireability, integrality, units, the search).

## Engineering — next

1. **Read the full-corpus sweep** (`tests/logs/unreach/full/`, engines
   `hscb` and `lp`, 430 models): `summary.py`; any wrong verdict is a bug
   to chase first. Then the report §2.7 and the pages.
2. **The cost of `S` and of the tests where the cap bites** (39 of 172
   sample runs): the zero-step selection of a cardinality atom summing
   places across the shape, the set's node count under a bad order, nets
   too big to compile in the budget (AirplaneLD-PT-2000). Instruments:
   `(stock S)`, the profile; a node budget on the construction; the
   atom's shape-friendliness.
3. **Paths on partially covered nets**: the reachable verdict of the
   backward search is taken only when no place was removed (an abstract
   path need not exist — two wrong verdicts in the full sweep, report
   §2.7). A second search on the *capped* set of the original net, in the
   main session, would give those paths back soundly: the converses are
   then the original events'. Worth 29 of the sample's 592 reachable
   verdicts (563 came from fully covered nets): low priority.
4. **Goals over removed places** (SupplyChain: 15 of 16 skipped): the
   place removed is the one the question reads. ω-values (ledger #2) or
   a bound from the LP (`Petri/src/lp`, "how high can this sum go") would
   cover it; the LP already answers those goals — the portfolio answer is
   to run both.
5. **The 304 searches cut at 2 s** on the sample: which layer count, which
   set sizes; a longer budget for the last open properties of a run; the
   first layer's `writing` restriction extended to later layers by the
   leaves the frontier's atoms read.
6. **The dead-transition rerun on BugTracking** through the abstraction
   pass under the projected order (running as this is written): the 170
   one-step kills of the capped run, now sound; against ITS-Tools' 1098
   SMT kills (61 s). Then k layers per slice.
7. **`--approx` before `R` in production** (the pass, then the fixpoint on
   the concrete net): the harness measurement (`BK_TOOL=hsc`), and the
   ITS-Tools `-rebuildPNML` early return on StateSpace (Application.java,
   right after `createSPN`) that hands `hscxred` the raw net.
8. Later: CTL `EF` leaves on `S`; a deadlock refutation on the abstract net
   (lost when a place is removed); traps and siphons as constraints;
   learning invariants from `S ∖ R` (ledger #17).

## Known

`(equality …)` must take its domains from the box (a bug found on
SupplyChain). The shape decides the set's cost: BugTracking's `S` has 8249
nodes under the original Sloan order projected, 39 587 under an order
recomputed on the abstract net. A `select` cut by a deadline raises through
the session: every timed test is wrapped, and counts as a timeout.
