# `src/` — non-header implementation

Mirrors `include/hsc/`. Anything that need not be visible to the compiler at
every use site belongs here rather than in a header; sources are listed
explicitly in `CMakeLists.txt` (no globbing).

By package:

* core — `diagram.cc`, `operation.cc` (the calculus engine),
  `stats.cc`, `timing.cc` (meters and clocks).
* leaves — `int_set.cc`: the enumeration-honest reference theory,
  extensional sets plus symbolic (`lia`) guards.
* lia — `lia_int.cc`, `lia_bool.cc`: interned arithmetic and boolean
  expressions, the interchange theory (see `include/hsc/lia/`).
* query — `query.cc`: cross-level criteria over diagrams — `split_equiv`
  on diagrams, `select_compare`, the separable per-position filters.
* surface — `surface_parser.cc` (T2M), `surface_translate.cc` (M2M): the
  `.hsc` file surface.
* petri — `petri_nupn.cc`, `petri_to_surface.cc`, `petri_decompose.cc`,
  and the vendored `louvain/`: PNML/NUPN import and unit-tree decomposition.
* dve — `dve_parser.cc` (T2M), `dve_to_surface.cc` (M2M + M2T): the BEEM
  front end (see `include/hsc/dve/`).
