# Handoff — ordering heuristics (`include/hsc/order/`, `experiments/order/`)

Current state only, next action first. Design: `experiments/order/SWEEP.md`
(pages, record, heuristics); package map: `include/hsc/order/README.md`.

## What stands

`sweep1` ran 1692 of its 4344 jobs (981 SS, 711 CTLC, 0 CTLF) before every
`tall` node went to other users' whole-node jobs; the 2651 waiting were
cancelled. Merged and reaped: `/data/ythierry/MCC26logs/hsc/sweep1/`
(`sweep1.tsv`, 40 614 rows, and the 615 MB of per-run logs). The read is
`SWEEP.md` §9.

It found a soundness bug before it found anything about shapes: **a verdict
off a partial reachable set is unsound** and the checker emits them
(§9.1) — 545 of 559 wrong CTLC verdicts come from a run that did not close
`R`, 47 % of all such answers are wrong. That is `handoff_ctl.md`'s to fix.
Until it is, no ranking in this sweep means what it says: the shapes that
look worst are the ones that fail to close `R` and then guess.

## Engineering — next

1. **Re-read the sweep once the checker is honest** — with unsound answers
   withheld, `answered` becomes comparable and §9.1's apparent ranking
   dissolves into a real one. Nothing else here is worth doing first.
2. **The 14 wrong on a complete `R`** (§9.2): `status=ok`, `R` closed, and a
   shape changed the verdict — `BridgeAndVehicles-PT-V10P10N10` and
   `-V20P20N10`, `Eratosthenes-PT-500` and `-200`,
   `DNAwalker-PT-12ringLLLarge`, `FileSystem-PT-N02I15B15`. A shape must
   never change an answer; each is a reproducer waiting to be cut down.
   Also the 3 wrong SS values (rule out `--cover` on unbounded nets first).
3. **The record's own defects** (§9.3): the `columns` header names 31 of 33
   fields (`reach_states` and `partial` unnamed); `crash` is 226 for `nupn`,
   205 `rcm`, 198 `force-rev`, 179 `random` against 1 for `force-iters`, and
   nothing yet says what they are.
4. **The pages** (`SWEEP.md` §2) — worth building on the partial record, as
   the instrument for the re-read above.
5. **The next sweep runs on `small%`.** 24 Westmere nodes, 576 cores, idle;
   `hsc-pn` runs there unmodified (PetriSpot `docs/CLUSTER.md` §1a). Budget
   about 10 concurrent jobs a node, not 24: 64 GB over 24 logical CPUs is
   2.7 GB a core against the 6 GB of §5. Wall times do not compare across
   node classes, but each job runs all 17 heuristics on one core, so the
   ranking within an instance does; record the class per row.
6. **The first epoch experiment** (`sched/algorithm.md` §3b): several
   heuristics for a fixed budget over `hsc FILE --stdin` with
   `(budget S) (reach R saturate) (stock R)`, compared across shapes by the
   leaf domains and the local states; then fuse what different shapes
   reached and continue from the best.
7. Later: the post-mortem reshape rule (profile, then bracket or move),
   the reduced-net population, Sloan weight variants.

## Known from the local runs

Shapes are complementary, not ordered: NUPN+FORCE, Louvain+FORCE, NUPN,
Louvain are each the unique best somewhere (27 / 25 / 15 / 4 runs of 588);
flows as cliques win TokenRing and SatelliteMemory and lose RwMutex; the
contraction of heavy flows wins every TwoPhaseLocking and loses every
RwMutex. Nodes kill a one-core job at about 6 GB; we work within it.
