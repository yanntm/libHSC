# `linear/` — linear constraints and diagrams, both ways

A client of the core, nothing more: from linear constraints over the leaves
(the P-flows of a net, a bound per place, any `Σ c·x = K` or `≤`) to the
diagram of every marking that satisfies them — an over-approximation of the
reachable set — and from a diagram back to constraints (a node exports as a
constraint set already; the reverse direction is what this package adds).
Then what such a set is good for: impossibility. A transition that no
marking of the invariant set enables is dead; one that no marking enables
in one step from a marking of the set that did not enable it is dead too
(one step of induction), the initial marking not enabling it. No SMT, no
dependency: the selectors build the set, one image tests the step.

| piece | what |
|---|---|
| `algorithm.md` | the construction, the tests, their soundness, their cost |
| `full.hh` / `src/linear/full.cc` | the product of the leaf domains as a diagram |
| `equality.hh` / `src/linear/equality.cc` | the diagram of a nonnegative linear equality over a box, built directly (a knapsack along the shape) |
| `dead.hh` / `src/linear/dead.cc` | the two deadness tests over an over-approximating set |
| `src/surface_linear.cc` (bindings) | `(full NAME [(LEAF LO HI)]*)`, `(equality NAME BOX K (* C LEAF)*)`, `(intersect NAME A B*)`, `(dead NAME SET [step] [ignore LEAF*])` |
| `hsc-pn --dead S [--dead-step]` (client) | flows within S seconds, structural zeros, box, equalities, tests; the dead transitions of the net (names under `-v`) |

First result: BugTracking-PT-q3m016, 24 601 of 27 370 transitions dead,
every one confirmed by the QuasiLiveness oracle, 15 s (`algorithm.md` §3).

Ideas ledger: `research_notes/ideas.md` #1 (this), #2 (ω-values for the
uncovered places), #3 (the same test on PetriSpot's LP).
