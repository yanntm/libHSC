# Handoff — CTL in libHSC

Current state only, next action first. Design: `research_notes/ctl_directions.md`
(the reference engine read against ours, the milestones). Rules of the
checker: `include/hsc/ctl/algorithm.md`; the core additions it needs:
`include/hsc/core/algorithm.md` §8 (gfp) and §9 (inverse with context).

## Engineering — next

1. **The inverse** (`core/algorithm.md` §9; M3 of the directions note).
   `int_set` local inverses with the declared domain as context — `keep`
   as is, `shift` with the guard translated, `assign` / `apply` / `havoc`
   through a new *havoc over a set* local term; structural inversion of op
   terms (`node`, `sum`, `compose`, `lfp`, `expr` refused loudly); the
   `pred(R) ⊆ R` test and per-event `meet(·, R)` protection. Then the model
   gets `pred_events`, `(pred NAME EVTERM SOURCE)` joins the surface, and
   the `restrict` leaves answer: `ctl_ring.hsc` P9 and `ctl_counter.hsc` Q6
   flip from `UNKNOWN` to their verdicts (P9 TRUE, Q6 TRUE).
2. **`hsc-pn` CTL examinations** (M4): vendored `CtlFormula` to `(ctl …)`
   text through `props_to_surface`; differential vs `its-ctl` on
   `examples/mcc` and PetriSpot `bench/models` `.solved`.
3. Then the optimisations on the produced questions: `has_image`, the
   constrained-closure rewrite (M5).

## Theory — open

* A saturation schedule for `gfp` (none known; breadth-first for now).
* The constrained-closure rewrite (`ctl_directions.md` §4.4) stated as a
  rule in `saturate()`'s vocabulary, with its commutation criterion.

## Done

* Directions note; `ctl/` docs; `core/algorithm.md` §8–§9.
* `ctl/formula.hh`, `ctl/forward.hh`: DAG, NNF, existential dual, the VIS
  rules to a question tree; the and-rule sends the right conjunct forward
  (a choice to re-examine, `ctl/algorithm.md` §4).
* `op_kind::gfp` in core; `ctl/checker.hh`: set expressions and backward
  `Sat` over an abstract model, saturated constrained closures, deadlock
  semantics, refusal without inverted events.
* Surface `(ctl …)`, `(expect-ctl …)`, `(gfp …)`, `(deadlock)`; manual §8f;
  `examples/models/ctl_ring.hsc`, `ctl_counter.hsc` (18 verdicts, ctest).
