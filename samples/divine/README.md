# samples/divine — the BEEM benchmark corpus

* `dve/` — 276 DVE models, 51 families: the patched BEEM set from ITSTools
  (`dve/fr.lip6.move.divine.xtext.tests/src/perfs/`), syntax errors of the
  original benchmark fixed there. Read-only inputs.
* `hsc/` — the same models through `dve2hsc` (T2M then M2M to surface forms,
  serialized). Regenerated, never hand-edited; kept in git so the surface's
  acceptance of each model is tracked over time — a model moving from a §7
  refusal to a passing run is the progress metric of the cross-level
  arithmetic thread.

The reachability properties (`.prop1.reach`) and libITS baseline timings for
these models live in `~/git/libITS/perfs/test_models`.
