# `dve/` — the DVE (BEEM) front end

DVE is the input language of DiVinE and of the BEEM benchmark: asynchronous
processes with explicit control states, shared byte/int variables and arrays,
and rendezvous channels. The corpus we target is the patched set in ITSTools
(`dve/fr.lip6.move.divine.xtext.tests/src/perfs/`, 276 models, 51 families);
the grammar reference is `Divine.xtext` beside it, and the semantic reference
for the mapping is `DveToGALTransformer.java` (DVE → GAL, same job aimed at a
different engine).

The pipeline is MDA-split like the surface, and stops at the surface's AST:

* **T2M** (`ast.hh`, `src/dve_parser.cc`) — text → a DVE model: declarations,
  processes, transitions, expression trees with *names*. Knows nothing of
  `hsc`. This model is the introspection point: alternative encodings and
  shape derivation (processes are the natural hierarchy candidates) read it.
* **M2M** (`src/dve_to_surface.cc`) — DVE model → surface `datum` forms, the
  same trees `.hsc` files parse to. Name resolution, state numbering,
  channel fusion, effect parallelization happen here.
* **M2T** — printing the datums is the `.hsc` serialization, for free.

A generated model runs through `surface::translate` like any `.hsc` file.
Today most BEEM models compile to forms the surface *refuses with a §7
pointer* (crossing assignments, arrays): that is the point — the corpus is
the target list for the cross-level arithmetic thread, each model turning
green as the engine grows into it.

`tools/dve2hsc.cc` drives it: parse, transform, print or run.
