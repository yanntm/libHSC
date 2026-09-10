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

Measured on the full local corpus (430 models, RC+RF, 13 760 formulas,
60 s and 6 GB a run): `S` + backward search **6796 answers, 0 wrong**;
PetriSpot's LP 3363, 0 wrong; complementary (`hscb` ahead on 541
instances, `lp` on 244). BugTracking: 854 sound one-step kills beyond the
24 601 structural ones, converging in 250 s. Report §2.12, §2.10.

## At shutdown (2026-09-10, midday)

* Running in the background when the session closed, unread:
  BugTracking's forward gfp (`tests/logs/unreach/dead_bt_gfp.err`: the
  image under the 1915 live events took 5.3 s, the gfp rounds follow) and
  the gfp sweep of the 86 sample models (`tests/logs/unreach/gfp/rows.tsv`,
  62 rows in; `experiments/unreach/gfp_sweep.sh`).
* The cluster campaign of `PetriSpot/Petri/test/mcc/campaign-2026-09-10.md`
  is **not submitted**: at 01:28 our account held 4314 jobs of the
  order-heuristics sweep; at 11:00, 2651 waiting and 0 running — the 19
  alive `tall` nodes are all taken by other users' whole-node jobs (lacour
  6, clusterbd 4, nibert 7 on 24 h jobs ending 09-11 morning, riwan 1,
  lemattre 1; tall14 dead), the scheduler's estimate for our next job is
  2026-09-14. Everything else is deployed and warmup-tested locally; the
  submit script is `submit-2026-09-10.sh`. The user decides whether to wait,
  cancel the sweep, or move to `small%`.

## Engineering — next

0. **The gfp of support** (ledger #23, report §2.13): read the two runs
   above; then the refutations on `G` against `S` and against the backward
   search on the sample; the two-way trim; the literature
   (`research_notes/gfp_support_litsearch.md` is the prompt; its answers go
   to the library and are studied in a session of their own).

1. **Pages for the campaign** (`MCC-analysis/campaign` style) from
   `experiments/unreach/results/*.tsv`, and the per-formula reading: which
   formulas the LP decides and `S` does not (the 244 instances), by atom
   shape and by coverage.
2. **The cost of `S` and of the tests where the cap bites** (76 capped and
   45 out-of-memory runs of 860): the zero-step selection of a cardinality atom summing
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
6. **BugTracking's remaining 1915 alive transitions**: the one-step test
   converges at 854 sound kills (250 s, report §2.10) against ITS-Tools'
   1098 by SMT in 61 s. `--dead-depth 2` exists and kills 144 of the 200
   smallest slices it reaches, at 4.5 s each (report §2.11): the second
   layer needs a restriction of its own (the leaves the first layer's
   markings differ on, or the survivors of depth 1 only under a
   per-candidate cap) before it is usable. Also: the cost of the first
   pass (100 s of the 250, the widest slices), and ITS-Tools' list of its
   1098 to see whether ours sit inside it.
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
