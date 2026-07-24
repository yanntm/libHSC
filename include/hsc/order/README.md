# `order/` — variable-ordering heuristics

The shape is a parameter of the representation, supplied from outside;
this package computes good frontier orders for it. The
calculus never guesses an order — a front end asks, then emits its shape.

## FORCE (`force.hh`)

> Aloul, F.A., Markov, I.L., Sakallah, K.A. (2003). FORCE: a fast and
> easy-to-implement variable-ordering heuristic. GLSVLSI '03.
> https://doi.org/10.1145/764808.764839

One-dimensional spring layout: every constraint pulls its variables toward
their common center of gravity; iterate, keep the cheapest order seen.
Re-expressed from libITS (`its/gal/force.cpp`, `GALVarOrderHeuristic.cpp`),
which drives the same algorithm from GAL transitions. Two constraint kinds:

* **clique** — variables one event touches, kept close. Cost: the span of
  the clique in the order. This is the classical FORCE hyperedge.
* **precedence** — an asymmetric pair `before < after`: free when
  satisfied, cost the distance when violated (libITS's `QueryEdge`, added
  there for the EquivSplit setting — our case bracket). A front end
  uses it to put what an event *reads* above what the read addresses: the
  curry then grounds residuals before reaching the cells they select, so
  a split's classes stay short-lived.

The caller extracts constraints (the DVE front end: one clique per event
over its touched leaves — a dynamic array access touches every cell — and
one precedence per (index or rhs read, addressed cell or written target)
pair), runs `force`, and emits its shape in the returned order. Order is
representation only: names resolve through whatever shape, so state counts
are invariant under it — which is the regression check.
