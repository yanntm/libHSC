# `order/` — variable-ordering heuristics

The shape is a parameter of the representation, supplied from outside;
this package computes good frontier orders and hierarchies for it. The
calculus never guesses an order — a front end asks, then emits its shape.
Everything that decides *where a variable sits* lives here, isolated from
the calculus and from the front ends, because it is the part we experiment
with most (`experiments/order/`): a heuristic is a function from a
dependency structure to an order or a tree, and its clients are adapters.

## Map

| piece | what |
|---|---|
| `force.hh` / `src/order/force.cc` | FORCE: a one-dimensional spring layout over cliques and precedences |
| `louvain/` | community detection (vendored Louvain) and `hyperedge.hh`, the bounds on what one hyperedge may contribute to the graph |
| `cluster.hh` / `src/order/cluster.cc` | a hierarchy over positions from weighted edges, with invariants as signed cliques and heavy ones contracted into flat nodes (`HSC_INV_*`) |
| `profile.hh` / `src/order/profile.cc` | the post-mortem instrument: nodes and arcs per sort of a computed set |
| `bandwidth.hh` / `src/order/bandwidth.cc` | reverse Cuthill–McKee and Sloan (bandwidth / profile reduction, LTSmin's finding), and the seeded random order as the control |
| `src/surface_reorder.cc` (client) | the directives `(reorder-force)`, `(reorder-reverse)`, `(flatten)` over a spec's shape |
| `src/surface_louvain.cc` (client) | the directive `(decompose-louvain)` over a spec's events |
| `petri/decompose.hh` (client) | a NUPN unit tree for a bare net: co-occurrence graph, the P-flows as hyperedges, the contraction of heavy-constant flows |
| `(profile NAME)` (`src/surface_query.cc`, client) | prints `profile.hh` with the spec's leaf names |

The shape is a value: the surface `(shape …)` expression over the leaf
names, written by `hsc-pn --export-shape`, read back by `--shape-file` or the
`(use-shape FILE)` directive — human-readable, diffable, tweakable, buildable
by any tool. `experiments/order/sweep_summary.py` reads a sweep's record.

Variation points, all environment variables read once: `HSC_INV_WEIGHT`,
`HSC_INV_CROSS`, `HSC_INV_MERGE` (the flows in the decomposition),
`HSC_FORCE_ITERS` (the spring iterations of `(reorder-force)`). What is
measured and what is next: `experiments/order/README.md`.

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
