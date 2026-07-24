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
* surface — `surface_parser.cc` (T2M); the symbolic translator
  (`surface_translator.hh` internal header, bodies in
  `surface_translate.cc` declarations/dispatch, `surface_events.cc` event
  compiler and algebra, `surface_families.cc` certified families,
  `surface_query.cc` commands); `surface_run.cc` the runner (public
  entry, knows both engines); `surface_spec.cc` declarations as data +
  domain analysis; `surface_rewrite.cc` the rewrite chain;
  `surface_xpl.cc` event forms → the explicit model.
* xpl — `xpl/eval.cc`, `xpl/model.cc`, `xpl/fire.cc`, `xpl/engine.cc`: the
  explicit engine — concrete evaluation, supports, the term interpreter,
  visitor-driven BFS (see `include/hsc/xpl/`).
* petri — `petri_nupn.cc`, `petri_to_surface.cc`, `petri_decompose.cc`,
  and the vendored `louvain/`: PNML/NUPN import and unit-tree decomposition.
* dve — `dve_parser.cc` (T2M), `dve_to_surface.cc` (M2M + M2T): the BEEM
  front end (see `include/hsc/dve/`).
