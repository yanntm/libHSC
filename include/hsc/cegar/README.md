# `cegar/` — certified component abstraction

A verification loop over finite LTS leaves synchronized by
product-shaped events: each leaf carries a certified over-approximation
of its trace language (a canonical classifier), the loop model-checks a
safety monitor against the classifiers, replays abstract witnesses by
executing their projections on the leaves (a failed execution *is* the
culprit), refines the culprit with an L\* learner, and ends with either
a concretely replayed violation or a certificate checkable without the
loop.

Scope: finite deterministic leaves, safety properties, explicit
walks, HSC-native.

## Placement

This package is the calculus core, parser-free: PODs and walks only,
depending on `util/` (hashing) alone. Models arrive already built —
the surface layer's bridge (`hsc/surface/cegar_build.hh`) derives
them from a parsed `.hsc` spec (the separable fragment: bounded
leaves, guarded-command events, per-leaf letters induced by domain
enumeration). The user surface is three commands of the main —
`(cegar …)`, `(certificate …)`, `(certcheck …)` — routed in
`surface_run.cc`; the checker's implementation lives surface-side
(`src/surface_certcheck.cc`) and shares this package's PODs and the
bridge, none of the loop.

## Files

* `model.hh` — `lts` (leaf: partial deterministic transition table,
  `fire` = membership, `value_of` back to `.hsc` values), `event`
  (support = per-leaf letters), `monitor` (complete DFA over events,
  derived by the bridge from the property atoms), `model` (the
  bundle) + `mono` (the monolithic oracle walk) and `refire`.
* `classifier.hh` — the canonical class table (shortlex reps, right
  action, live mask, one absorbing dead class), `canonicalize`
  (merge dead, Moore-minimize, shortlex-rename), `chaos`, byte
  serialization (equality = byte equality).
* `certify.hh` — the teacher's certification walk: `lts × classifier`
  product BFS; certified, or a positive counterexample.
* `learner.hh` — L\* with Rivest–Schapire counterexample handling;
  `publish()` returns the dead-closed, canonicalized hypothesis. The
  observation table is internal state, never an abstraction: the
  loop consumes only published, certified classifiers (the
  publication discipline).
* `loop.hh` — profile (interned per distinct leaf), abstract-product
  search, replay by projection, refine-until-resolved, `run`;
  certificate emission as `.hsc` s-expression text.

Sources: `src/cegar/` (static library `hsc_cegar`). Tests:
`tests/cegar/` (own doctest binary; fixtures are `.hsc` literals).
Model families: `examples/cegar/` (model-only files + drivers).
