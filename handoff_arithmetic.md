# Handoff — cross-level arithmetic

**Objective: close the performance gap on BEEM.** 168/276 models agree
exactly with its-reach within 5 s; the ~108 timeouts are the work list;
`examples/divine/runs/` is the instrument. The frontier is engine cost,
not expressiveness.

## Engineering queue (next action first)

1. **Profile the timeout population.** Pick 3–5 timing-out families
   (`adding`, `anderson`, `bakery` are big-array/big-domain heavy); find
   where the time goes: G-event chaining under saturation, split_equiv
   recomputation per (term, class), xform enumeration vs the shift fast
   path lost to byte-wrap `%` chains (a theory-level wrapping shift would
   restore it), cache pressure (R2). Fix the biggest one first.
2. **FORCE knobs, measured.** The precedence direction and clique weights
   are guesses; A/B them with labeled sweeps
   (`tests/dve_sweep.sh 5 <label> --order=…`). Try balanced shapes over
   the FORCE order, and Louvain-grouped subtrees for DVE (exists for
   NUPN only).
3. **`.prop1.reach` criteria** (`~/git/libITS/perfs/test_models/*.reach`)
   → `(select …)` queries: turns the corpus into reachability-property
   benchmarks, not just state counts.
4. **`ite` statement** — the do-language (`assign | ite | seq`) is still
   missing `ite`; DVE never needed it. Add it when a front end asks.

## Theory queue

1. Fold into the paper if wanted: the strong-Kleene deviation note
   (`lia/algorithm.md`) and the placement-vector array encoding.
2. Composite cells (`tab[i].x`): declaration syntax and writes need
   design; per-field placement vectors give the reads almost free.

## Blockers

None.

## Explicitly not now

Benchmark campaign at real budgets (≥60 s, memory metering), the R2
memory wall, the fast FDD leaf (R3), P-invariant bounds, `trans_*`
transient semantics.
