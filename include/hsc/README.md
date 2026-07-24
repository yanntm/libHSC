# `include/hsc/` — the public headers

Layer order, each folder knowing only the ones above it in this list:

* `util/` — primitives with no libHSC concepts in them: hashing, small
  helpers. Depends on nothing.
* `mem/` — the interning and caching substrate. Depends on `util/`.
  **Knows nothing about diagrams**: it interns and caches values of a type
  parameter, whatever that type is.
* `core/` — the calculus itself: shapes, the leaf-theory concepts, diagram
  and operation declarations.
* `lia/` — the interchange theory: interned integer and boolean expressions
  over positions. Depends on `mem/` and `util/` only.
* `leaves/` — leaf theories (`int_set`, symbolically guarded via `lia/`).
* `query.hh` — cross-level criteria over diagrams: `split_equiv` lifted to
  diagrams, `select_compare` (the crossing case), the per-position filters. Sits
  above `core/` and `leaves/`.
* `surface/` — the `.hsc` file surface: s-expression parser (T2M) and
  translator (M2M). The only layer that gives text operational meaning.
* `petri/` — PNML/NUPN import and Louvain decomposition, emitting surface
  text.
* `dve/` — the DVE (BEEM) front end: parser to a DVE model (T2M), transform
  to surface forms (M2M), `.hsc` serialization (M2T).

Non-header implementation lives in `src/`, mirroring these names.

Two rules that hold across the whole tree:

* Everything interned defines `hash()` and `operator==`. Composite hashes go
  through `util::combine`; XOR of member hashes is banned.
* No singletons. Tables and caches are owned by a manager and reach the code
  that uses them as explicit context arguments.
