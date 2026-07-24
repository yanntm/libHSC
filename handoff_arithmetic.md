# Handoff — cross-level arithmetic

Current state and the next objective. Rewritten in place; no history here.

## The objective

**Close the performance gap on BEEM.** Semantics is settled and validated:
every model compiles, 168/276 run within 5s and all 168 agree exactly with
its-reach on the same files. its-reach (15s, mature engine) solves 50 more;
we solve none it cannot. The frontier is now engine cost, not
expressiveness — the 108 timeouts are the work list, and
`examples/divine/runs/` (per-model states/nodes/seconds each sweep) is the
instrument.

## Engineering queue (next action first)

1. **Profile the timeout population.** Pick 3–5 timing-out families
   (`adding`, `anderson`, `bakery` are big-array/big-domain heavy); find
   where the time goes: G-event chaining under saturation, split_equiv
   recomputation per (term, class), xform enumeration vs the shift fast
   path lost to byte-wrap `%` chains (a theory-level wrapping shift would
   restore it), cache pressure (R2). Fix the biggest one first.
2. **FORCE knobs, measured.** The precedence direction (reads above cells
   vs below) and clique weights are guesses; A/B them with labeled sweeps
   (`tests/dve_sweep.sh 5 <label> --order=…`). Try balanced shapes over
   the FORCE order, and Louvain-grouped subtrees for DVE (exists for
   NUPN only).
3. **`.prop1.reach` criteria** (`~/git/libITS/perfs/test_models/*.reach`)
   → `(select …)` queries: turns the corpus into reachability-property
   benchmarks, not just state counts.
4. **`ite` statement** — the settled do-language (`assign | ite | seq`) is
   still missing `ite`; DVE never needed it. An `ite` whose condition
   crosses IS the §7 case bracket; add it when a front end asks.

## What stands under this (validated, in the packages)

* **The §7 case bracket** (`hsc/event.hh`): crossing guards and updates as
  `op_kind::expr` G-events — build pushes down and re-roots, apply curries
  by `split_equiv`, residuals interned and cached. Verified against brute
  force on spine and balanced shapes, including `tab[tab[x]]` (R4 gate
  met) and simultaneous swaps.
* **Arrays carry their placement**: a `lia` array node holds each cell's
  frontier position (spread/permuted free; a variable is the degenerate
  access, constant index folds, OOB is ⊥ = abort = the algebra's 0).
  Wanted next, not built: composite cells (`tab[i].x` — per-field
  placement vectors give the reads almost free; declaration syntax and
  writes need design).
* **Strong Kleene connectives** in `lia`: `false ∧ ⊥ = false`,
  `true ∨ ⊥ = true`; De Morgan-sound, observationally DVE's lazy `&&`.
  The old ⊥-poisoning claim ("GAL's semantics") was wrong — see
  `lia/algorithm.md` for the deviation note.
* **The event algebra**: `event`/`alt`/`seq` declare named terms (sum,
  compose), nesting inline plus anonymous `(when …)`/`(do …)` atoms;
  `(reach R [strategy] [EVTERM] [from RESULT])` takes the system, default
  = ALT of every event. Dead event = zero term. Queries compose over
  bound results (`states`/`count`/`nodes`/`max-value`/`select`);
  `deadlock` and `one-safe` forms are gone — MCC protocol lives in
  `hsc-mcc`.
* **The state layer**: initialization is an event applied once to the
  base word (leaves at LO) — `alt` for several initial states, `havoc`
  ranges (one theory term), havoc-then-`when` for regions by predicate;
  an empty init image is a refused model. States are results: `word`
  literals, `apply` one-step images, `get-witness`/`get-states` exhibit
  states as re-runnable word literals.
* **DVE fidelity**: byte wrap mod 256 (M2M-emitted `%` chains), rendezvous
  value on the pre-state with receiver-before-sender effects (the
  operative dve2gal order). Both were found by differential against
  its-reach — keep that oracle current with
  `tests/collect_its_baseline.sh`.
* **FORCE ordering** (`hsc/order/`): cliques per event + asymmetric
  read-before-cell precedences; +25 models at the same cap, node counts
  down ~40% where measured. Order is representation only — counts
  invariant, which is the standing regression check
  (`tests/compare_beem_baseline.sh`, 168/168).

## Blockers

None. The theory side may want to fold into the paper: §7 is no longer
"remains to be built" (hsc_core.md §7 closing line), the Kleene deviation
note, and the placement-vector array encoding.

## Explicitly not now (exists, or secondary)

Benchmark campaign at real budgets (≥60s, memory metering), the R2 memory
wall, the fast FDD leaf (R3), P-invariant bounds, composite-cell arrays,
`trans_*` transient semantics (noted per file, not reproduced).
