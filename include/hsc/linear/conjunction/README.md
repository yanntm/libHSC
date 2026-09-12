# Conjunction of linear selectors

Implemented by `filter.hh` and `src/linear/conjunction/filter.cc`. [algorithm.md](algorithm.md) specifies a
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
from leaf domains; the conjunction evaluator preserves and filters the
arcs of its input diagram.

Development starts from this specification. Validate the implementation on real
models, beginning with Anderson and smaller instances of that family, using
bounded runs and the existing oracle. No new synthetic benchmark or collection
of small regression tests is part of this work.

The public `linear::conjunction` API takes a manager, its integer leaf theory,
a rooted shape, an input diagram, and sparse constraints. It inherits the
manager's absolute deadline and returns an exact result or throws. The surface
binding is `(constrain NAME SOURCE (eq K (* C LEAF)*) (le K (* C LEAF)*) ...)`.
The approximation client uses this binding to submit all retained constraints.

Memo tables use canonical diagram handles. Schedule caches are scoped to one
coordinate-specific schedule; knapsack and contribution caches to one constraint.
Residuals of different constraints do not yet share a memo table. Shape widths
are cached as well. No general Boolean-expression rewrite is installed.

`tests/conjunction_model.cc` is one optional, real-model comparison executable:
`cmake --build build --target conjunction_model`, then
`timeout -k 1 15 build/tools/conjunction_model MODEL.pnml`.
It compares canonical diagrams with the independent-equality construction in
the same manager on Sloan spines and balanced shapes, then checks filtering an
already correlated result with reordered constraints and deadline expiry.
It expects the model's places to have finite bounds; it does not implement the
approximation client's projection policy. It is not part of the default test run.

## Real-model observations

Anderson PT04 agrees exactly with the independent-equality construction on
both Sloan spines and balanced shapes, including filtering correlated inputs
and changing constraint order. Anderson PT05 returns the same four StateSpace
values with and without FORCE: 689901 states, 2784245 transitions, maximum
place marking 1 and maximum total marking 7.

With the same local binary, PT05's invariant set contains 2195405 markings:

| Invariant spine | Nodes | Construction seconds | Total reduction seconds |
|---|---:|---:|---:|
| Sloan | 39017 | 0.516 | 3.10 |
| Sloan followed by FORCE | 72965 | 0.640 | 5.61 |

These are single bounded runs, not a statistical benchmark. FORCE here uses
the model's event supports, not the flow-constraint supports. It preserves
the spine hierarchy. `--force` now applies to the approximation and reduction
sessions as well as reachability. The new evaluator has not demonstrated a
speedup over the previous equality construction on these small Sloan examples.
Logs are under `/data/ythierry/MCC26logs/local/conjunction/`.
