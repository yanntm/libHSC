# Handoff — CTL in libHSC

Current state only, next action first. Design: `research_notes/ctl_directions.md`
(the reference engine read against ours, the milestones). Rules of the
checker: `include/hsc/ctl/algorithm.md`; the core additions it needs:
`include/hsc/core/algorithm.md` §8 (gfp) and §9 (inverse with context).

## Engineering — next

1. **`op_kind::gfp`** in `core/operation.hh` + `do_apply`: `X ↦ X ∩ h(X)`
   from the argument downward, breadth-first (§8). Differential test:
   `gfp(next)·R` on a model with and without a cycle.
2. **The checker** (`ctl/checker.hh`, `src/ctl/checker.cc`): evaluate set
   expressions and backward `Sat` per `algorithm.md` §3–§4 over an abstract
   model — sort, `R`, seed, forward event terms, inverted event terms (may be
   empty), atom → selector term, deadlock selector. Memo per set id / node
   id. A `restrict` leaf whose formula has a path operator refuses when the
   inverted events are absent: verdict `unknown`, never a guess.
3. **Surface**: `(ctl NAME FORMULA)` with atoms as query atoms and
   `(deadlock)`; `(expect-ctl NAME TRUE|FALSE)`. Self-checking `.hsc` files
   under `examples/models/` as the tests; the explicit engine has no CTL, so
   the oracle for small models is hand-checked formulas and `its-ctl`.
4. **The inverse** (M3 in the directions note): `int_set` local inverses
   with the declared domain as context, `havoc` over a set as the new local
   term, structural inversion of op terms, refusal of case brackets,
   `pred(R) ⊆ R` test and per-event protection. Then `(pred NAME EVTERM
   SOURCE)` and the `restrict` leaves answer.

## Theory — open

* A saturation schedule for `gfp` (none known; breadth-first for now).
* The constrained-closure rewrite (`ctl_directions.md` §4.4) stated as a
  rule in `saturate()`'s vocabulary, with its commutation criterion.

## Done

* Directions note; `ctl/` docs; `core/algorithm.md` §8–§9.
* `ctl/formula.hh` (hash-consed DAG, NNF, existential dual, `is_state`,
  `convertible`) and `ctl/forward.hh` (the VIS rules to a question tree of
  `nonempty?` leaves under `any`, polarity by convertibility, several
  initial states force the negated question). `tests/test_ctl.cc`.
