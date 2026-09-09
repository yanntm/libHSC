# `petri/` — PNML/NUPN import

The path from a Petri net file to the `.hsc` surface. It emits **text**: the
importer does not link the calculus core. Used by `tools/nupn2hsc`.

## Vendored, with attribution

The subfolders `core/`, `parse/`, `expr/`, `io/` are **exact copies** of the
same-named folders of PetriSpot's `Petri/src`
(<https://github.com/yanntm/PetriSpot>, © Yann Thierry-Mieg,
GPL-3.0-or-later): solid, battle-tested code, and the sparse-matrix
representation is the substrate for any future endogenous rewriting of the
net (structural reduction, agglomeration) before export. No file is edited
here: what the copies need is changed upstream first, then `vendor.sh`
re-copies the list and checks every file is byte-identical to its source.
Upstream's layout is kept so that their mutual includes (`"core/…"`,
`"expr/…"`) resolve verbatim; this folder is an include root of the `hsc`
and `petri_import` targets for that reason. Someday these become a library
shared by the two projects.

* `core/` — `SparseArray.h`, `MatrixCol.h` (sparse column matrix, the flow
  matrices), `SparseBoolArray.h`, `Arithmetic.hpp`, `InvariantHelpers.h`,
  `Rational.h` (their support; overflow-checked arithmetic is deliberate
  robustness), `SparsePetriNet.h` (the net: names, marking,
  `flowPT`/`flowTP`), `Log.h` (the timestamped log line; the tools redirect
  it to stderr with `setLogStream`, stdout being the emitted model or the
  answer protocol).
* `parse/` — `PTNetHandler.h`, `PTNetLoader.h` (the expat SAX parser for PNML
  P/T nets), `NetResolver.h` (names and `p<i>`/`t<i>` indices against a net),
  `PropertyFile.h` (property syntax by extension); `parse/mcc/` the expat
  handler for the MCC property XML, CTL included; `parse/sexpr/` the
  s-expression reader (`Sexpr.h`, itself a copy of our `surface/sexpr.hh`),
  `PropertyReader.h` (forms to properties, the grammar of PetriSpot's
  `INTEROP.md`), `HintReader.h`.
* `expr/` — the property tree: `Expression.h` (linear atoms over places,
  booleans), `CtlFormula.h`, `CtlSimplify.h` (CTL over those atoms),
  `Property.h` (a named property and its kind: reach, invariant, deadlock,
  bound, CTL), `Simplify.h`, `Hint.h` (Parikh hints), `SexprPrinter.h` (the
  tree as s-expressions, indices or names). Its `README.md` and
  `algorithm.md` are upstream's.
* `io/` — `SparseMatrixIO.h`, `PNETIO.h`: KERS matrices and the PNET binary
  net container, with `io/PNET.md` specifying that container (header, the
  three mandatory blocks, and the optional named blocks that say what a
  producer knows about the net's provenance).

The invariant *solver* (`InvariantMiddle`/`Calculator`/`Heuristic`/`RowSigns`),
the walk engine and the CTL checker are **not** vendored.

## Ours

* `nupn.hh` + `src/petri_nupn.cc` — the NUPN unit tree. `PTNetHandler` treats
  `toolspecific` as opaque, so a second SAX pass reads it; logic mirrors
  `fr.lip6.move.gal.nupn.NupnHandler`. A unit holding both places and subunits
  is kept as-is (bracketed together), not split into a synthetic child.
* `invariants.hh` + `src/petri_invariants.cc` — the P-flows of a net through
  the vendored calculator (`invariants/`, PetriSpot's `InvariantMiddle`), as
  supports with coefficients and the constant the initial marking fixes; a
  deadline, no compression, flows rather than semiflows (at most |P| of them).
* `decompose.hh` + `src/petri_decompose.cc` — a unit tree for a net that
  arrives without one, by Louvain clustering over a place co-occurrence graph
  (control→write per transition, all-to-all as fallback). The graph is built
  from the net alone: no property takes part, so nothing can make one community
  mandatory. Given the flows, each one's support is one more hyperedge (weight
  `HSC_INV_WEIGHT`, default 1, shared over its pairs, bounded like a
  transition's): places an invariant ties together attract each other, which
  is what keeps a token conservation from straddling a cluster frontier. `louvain/hyperedge.hh` bounds what a single transition may
  contribute — a hyperedge of support k relates its places pairwise at a cost
  quadratic in k, and a transition wide enough to matter is a synchronisation
  rather than a progression, so it is left out instead of expanded.
* `to_surface.hh` + `src/petri_to_surface.cc` — M2T. One leaf per place, the
  shape read off the unit tree verbatim (a unit → a spine of its places then its
  subunits; a single child collapses), one event per transition (guard = input
  arcs `>= w`, action = net per-place effect). Reachability is composed from the
  generic algebra. Every P/T transition is separable (Cor 6.3), so nothing here
  needs `split_equiv`.

## Assumptions

The leaf domain is `[0, bound)`, `bound = max(2, max initial marking + 1)` — a
1-safe NUPN needs only `2`. A non-safe net that grows a place past the bound is
outside this iteration (`--bound` raises it); convergence of an unbounded place
under closure is the net's responsibility (hazard H2).
