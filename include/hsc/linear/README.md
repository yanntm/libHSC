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
| `src/surface_linear.cc` | `(full NAME)`: the product of the leaf domains; `(dead NAME SET)`: the events dead on SET, by enabling and by one step |
| `hsc-pn --dead S` (client) | the flows within S seconds → bounds and constraints → the invariant set → the dead transitions of the net, reported |

Ideas ledger: `research_notes/ideas.md` #1 (this), #2 (ω-values for the
uncovered places), #3 (the same test on PetriSpot's LP).
