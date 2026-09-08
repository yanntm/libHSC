# `ctl/` — CTL over a reachable diagram

A CTL checker: formulas over opaque atoms, put in negation normal form,
then in the **forward form** of Iwashita et al. (VIS), and evaluated as a
tree of emptiness questions over set expressions. Every question is spelled
in the calculus's own algebra — `join / meet / minus`, one-step images,
`lfp` from a seed over a filtered event term, the deflationary closure
`gfp` — so the checker adds no operation of its own; it only decides which
questions to ask and in what order. `algorithm.md` beside this file states
the rules.

## Placement

Core-side and parser-free: the package depends on `core/` and `util/` and
knows nothing about Petri nets, `.hsc` text or the MCC XML. Atoms are
indices into a table the caller owns; the caller supplies, per atom, the
**selector term** (a guard-only event) that keeps the states satisfying it,
and the model: the sort, the reachable set, the seed, the forward event
terms and — when it has them — the inverted ones. The surface layer binds
the two (`(ctl NAME FORMULA)` and the `hsc-pn` driver); the vendored
`petri/expr/CtlFormula.h` is the MCC input this package's tree is built
from, not a dependency.

## Files

* `formula.hh` — the formula DAG: atoms, constants, `not / and / or`, the
  ten path operators (`EX AX EF AF EG AG EU AU EW AW`); hash-consed so a
  subformula shared by several properties is one node and one memo entry.
  Negation normal form and the existential dual of a node.
* `forward.hh` — the forward conversion: from `(seed, φ)` to a **question
  tree** whose leaves are `nonempty?(set expression)` and whose set
  expressions are `init`, `ey`, `fwdu`, `fwdg`, a state filter, or a
  restriction by a formula that has to be evaluated backward.
* `checker.hh` — evaluation: set expressions and `Sat` memoised per node,
  the backward operators as their fixpoints over the inverted events, the
  deadlock-terminated path semantics of the contest, the verdict.

Sources: `src/ctl/`. Tests: `tests/test_ctl.cc` (the conversion on hand
formulas) and the self-checking `examples/models/ctl_ring.hsc`,
`ctl_counter.hsc` (verdicts hand-checked, through the surface's `(ctl …)`).
