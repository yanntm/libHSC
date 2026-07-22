# `include/hsc/` — the public headers

Layer order, each folder knowing only the ones above it in this list:

* `util/` — primitives with no libHSC concepts in them: hashing, small
  helpers. Depends on nothing.
* `mem/` — the interning and caching substrate. Depends on `util/`.
  **Knows nothing about diagrams**: it interns and caches values of a type
  parameter, whatever that type is.
* `core/` — the calculus itself: shapes, the leaf-theory concepts, diagram
  and operation declarations. (M1, second half.)
* `leaves/` — leaf theories. (M2+.)
* `ops/` — operation terms. (M4+.)

Non-header implementation lives in `src/`, mirroring these names.

Two rules that hold across the whole tree:

* Everything interned defines `hash()` and `operator==`. Composite hashes go
  through `util::combine`; XOR of member hashes is banned.
* No singletons. Tables and caches are owned by a manager and reach the code
  that uses them as explicit context arguments.
