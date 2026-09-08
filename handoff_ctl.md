# Handoff — CTL in libHSC

Current state only, next action first. Design: `research_notes/ctl_directions.md`
(the reference engine read against ours, the milestones). Rules of the
checker: `include/hsc/ctl/algorithm.md`; the core additions it needs:
`include/hsc/core/algorithm.md` §8 (gfp) and §9 (inverse with context).

## Engineering — next

1. **`hsc-pn` CTL examinations** (M4): `PropertyKind::CTL` in
   `tools/pn_solver.hh` — the vendored `CtlFormula` rendered as `(ctl NAME …)`
   text through `props_to_surface` (atoms as `select` atoms, `Deadlock` as
   `(deadlock)`), the verdict read back from the session's `NAME ctl …` line;
   `UNKNOWN` stays silent (no `FORMULA` line). Check against the `.solved`
   oracles of PetriSpot `bench/models/*/CTL*` and add CTL samples to the
   `pn_samples` test.
2. **Measure** the inverse on the MCC nets: how many events get protected by
   `within(R)` per model, backward set sizes vs `R`, time of `invert_events`.
3. Then the optimisations on the produced questions: `has_image`, the
   constrained-closure rewrite (M5).

## Theory — open

* A saturation schedule for `gfp` (none known; breadth-first for now).
* The constrained-closure rewrite (`ctl_directions.md` §4.4) stated as a
  rule in `saturate()`'s vocabulary, with its commutation criterion.

## Done

* Directions note; `ctl/` docs; `core/algorithm.md` §8–§9;
  `research_notes/invert.md`.
* `ctl/formula.hh`, `ctl/forward.hh`: DAG, NNF, existential dual, the VIS
  rules to a question tree; the and-rule sends the right conjunct forward
  (a choice to re-examine, `ctl/algorithm.md` §4).
* `op_kind::gfp`, `op_kind::within`; `core::inverter` (structural converse
  relative to a potential); `support_algebra::invert_local` (optional,
  default refuses); `int_set` `choose` and `invert_local`.
* `ctl/checker.hh`: set expressions and backward `Sat`, saturated
  constrained closures, deadlock semantics, refusal without inverted events.
* Surface `(ctl …)`, `(expect-ctl …)`, `(gfp …)`, `(invert …)`,
  `(deadlock)`; manual §8f; `examples/models/ctl_ring.hsc`,
  `ctl_counter.hsc` (21 verdicts + backward checks, ctest); the inverse
  differential over 150 random models in `tests/test_operations.cc`.
