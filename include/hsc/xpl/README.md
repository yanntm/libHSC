# `xpl/` — the explicit engine

Concrete states, enumerated one at a time: the explicit complement to the
symbolic calculus. Different strengths at different moments — thousands to
a million states, exact counts, witnesses as runnable word literals, and
loud, precise errors where the symbolic engine folds silently. Uses: an
independent oracle for the symbolic differential chain, model debugging,
and (to come) guided walks and simulation.

The name: `explicit` is a C++ keyword, so the package is `xpl`.

## Isolation

`xpl` never sees a diagram. It depends on:

* `lia/` — expressions are *read* through `expr_factory`'s const API; no
  interning traffic during a run;
* `util/` — hashing;
* the sorted sparse containers vendored under `petri/`
  (`SparseBoolArray.h`) — self-contained headers, no Petri semantics.

The bridge to the rest of the system is the surface: `surface/xpl_build.hh`
compiles event forms into an `xpl::model`, and the translator seeds a run
by enumerating a symbolic result into concrete states (`xreach`, optionally
`from` any bound result — a witness, a select). Both directions cross in
surface code; nothing here includes `core/`.

## Files

* `state.hh` — `value` (int32), `state_id`, `state_view`.
* `store.hh` — the state store: fixed-arity rows, deduplicated,
  open-addressed; insert-if-absent is the one operation.
* `interpret/` — the transition relation: `model.hh` (events as `lia`
  codes over frontier positions), `eval.hh` (concrete evaluation, loud
  errors), `fire.hh` (enabledness and successors). See
  `interpret/algorithm.md`.
* `engine.hh` — reachability: BFS with enabled-set maintenance. See
  `algorithm.md`.

New capabilities (strategies, walks, on-the-fly properties) arrive as new
submodules beside `interpret/`, not as growth of the engine.

## Concurrency posture (designed for, not built)

Parallel explicit search is nearly free and the interfaces are cut for it:

* the model and the expression factory are immutable during a run —
  shared read-only;
* evaluation and firing are pure functions of (model, state) with
  per-call scratch — per-thread by construction;
* the store's single operation, insert-if-absent, is the only
  linearization point: shard it by hash;
* frontier entries are self-contained (state id + enabled set) — a work
  queue splits into per-thread deques with stealing;
* an error latches the first diagnostic and stops the run — a flag, not
  a data structure.

Nothing in the engine holds hidden mutable state; `reach` is a function.
