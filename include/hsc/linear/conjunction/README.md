# Conjunction of linear selectors

Design, not yet implemented. [algorithm.md](algorithm.md) specifies a
shape-directed evaluation of a conjunction of linear constraints against an
existing diagram. Constraints are scheduled at their lowest spanning shape
node; a knapsack filters the current diagram there, after the constraints
strictly below it have been applied.

The first integration is a specialised API for the invariant-set construction
used by the approximation and dead-transition passes. A later compiler rewrite
may select the same evaluator for a conjunction of recognised pure selectors.
The algorithm does not depend on Petri-net semantics.

Existing machinery: [linear constraints](../algorithm.md),
[the equality constructor](../equality.hh), and
[budgets](../../sched/algorithm.md). The existing equality constructor builds
from leaf domains; the proposed evaluator must instead preserve and filter the
arcs of its input diagram.

Development starts from this specification. Validate the implementation on real
models, beginning with Anderson and smaller instances of that family, using
bounded runs and the existing oracle. No new synthetic benchmark or collection
of small regression tests is part of this work.
