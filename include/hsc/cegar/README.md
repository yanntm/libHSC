# `cegar/` — certified component abstraction

A verification loop over a tree of finite LTS leaves synchronized by
product-shaped events: each leaf carries a certified over-approximation
of its trace language (a canonical classifier), the loop model-checks a
safety monitor against the classifiers, replays abstract witnesses by
executing their projections on the leaves (a failed execution *is* the
culprit), refines the culprit with an L\* learner, and ends with either
a concretely replayed violation or a certificate checkable by an
independent program.

Paper: `research_notes/cegar_certified_abstractions_v2.md`. Spec:
`research_notes/cegar_spec.md` (v1 scope: finite leaves, safety,
explicit walks).

## Isolation

Depends on `util/` (hashing) only — no `core/`, no `lia/`, no
`surface/`, no `xpl/`. The package brings its own tiny model format
(`.cts`) and generator; bridges to the model corpora are a later
thread.

## Files

* `model.hh` — `lts` (leaf: partial deterministic transition table,
  `fire` = membership), `event` (support = per-leaf letters),
  `monitor` (complete DFA over events, missing entries self-loop),
  `shape` (binary tree), `model` (the bundle) + `.cts` parse/print.
* `classifier.hh` — the canonical class table (shortlex reps, right
  action, live mask, one absorbing dead class), `canonicalize`
  (merge dead, Moore-minimize, shortlex-rename), `chaos`, byte
  serialization (equality = byte equality).
* `certify.hh` — the teacher's certification walk: `lts × classifier`
  product BFS; certified, or a positive counterexample.
* `learner.hh` — L\* with Rivest–Schapire counterexample handling;
  `publish()` returns the dead-closed, canonicalized hypothesis.
* `loop.hh` — profile (interned per distinct leaf), abstract-product
  search, shape-descent replay, refine-until-resolved, `run`,
  certificate emission (`.cert`); `mono` (the monolithic oracle walk)
  and `refire` live in `model.hh`.
* `gen.hh` — model families (clients, clients-bug, ring, rand),
  deterministic in a seed.

Sources: `src/cegar/` (own static library `hsc_cegar`). Binaries:
`tools/cegar/` — `hsc-cegar` (run / mono / gen) and `hsc-certcheck`,
the independent checker, deliberately sharing **no code** with this
package.

Tests: `tests/cegar/` (own doctest binary). Experiment records:
`experiments/cegar/`.
