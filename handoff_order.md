# Handoff — ordering heuristics (`include/hsc/order/`, `experiments/order/`)

Current state only, next action first. Design: `experiments/order/SWEEP.md`
(pages, record, heuristics); package map: `include/hsc/order/README.md`.

## Engineering — next

1. **Read the first sweep** (`sweep1`, cluster: 1681 P/T instances × SS,
   CTLC, CTLF × 17 heuristics at 300 s, one core each). Reap it into
   `/data/ythierry/MCC26logs/hsc/sweep1/` (rsync of `results/sweep1.tsv`
   and `results/sweep1/`), run `sweep_summary.py`, look for execution
   issues first (statuses `crash`, `memory`, `timeout`, wrong values), then
   the unique wins and the dominated heuristics. Two `oarsub` calls failed
   (OAR database error): reconcile the missing (instance, exam) pairs.
2. **The pages** (`SWEEP.md` §2) once the logs are down: heuristics against
   each other, pairwise matrix, instance heatmap with metric selector,
   scatter, cactus, families, one instance in depth (shapes diffed from the
   `.shape` files beside the logs).
3. **The first epoch experiment** (`sched/algorithm.md` §3b): several
   heuristics for a fixed budget over `hsc FILE --stdin` with
   `(budget S) (reach R saturate) (stock R)`, compared across shapes by the
   leaf domains and the local states; then fuse what different shapes
   reached and continue from the best.
4. Later: the post-mortem reshape rule (profile, then bracket or move),
   the reduced-net population, Sloan weight variants.

## Known from the local runs

Shapes are complementary, not ordered: NUPN+FORCE, Louvain+FORCE, NUPN,
Louvain are each the unique best somewhere (27 / 25 / 15 / 4 runs of 588);
flows as cliques win TokenRing and SatelliteMemory and lose RwMutex; the
contraction of heavy flows wins every TwoPhaseLocking and loses every
RwMutex. Nodes kill a one-core job at about 6 GB; we work within it.
