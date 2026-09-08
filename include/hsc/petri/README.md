# `petri/` — PNML/NUPN import

The path from a Petri net file to the `.hsc` surface. It emits **text**: the
importer does not link the calculus core. Used by `tools/nupn2hsc`.

## Vendored, with attribution

These files are copied from **PetriSpot**
(<https://github.com/yanntm/PetriSpot>), © Yann Thierry-Mieg, GPL-3.0-or-later,
and kept close to source deliberately — solid, battle-tested code, and the
sparse-matrix representation is the substrate for any future endogenous
rewriting of the net (structural reduction, agglomeration) before export.
`vendor.sh` re-copies them from a PetriSpot checkout: it prepends the
provenance banner, rewrites the include paths to this layout and re-applies
the local edits below, so a sync with upstream is one run and one commit.

Flat in this folder (upstream `core/` and the PNML loader of `parse/`):

* `SparseArray.h`, `MatrixCol.h` — sparse column matrix (the flow matrices).
* `SparseBoolArray.h`, `Arithmetic.hpp`, `InvariantHelpers.h`, `Rational.h` —
  their support (overflow-checked arithmetic is deliberate robustness).
* `SparsePetriNet.h` — the net: names, marking, `flowPT`/`flowTP`.
* `PTNetHandler.h`, `PTNetLoader.h` — the expat SAX parser for PNML P/T nets.
* `Log.h` — the timestamped log line the loader prints.

In their upstream subfolders (their mutual includes stay verbatim; the
`petri_import` target has this folder as a second include root):

* `expr/` — the property tree: `Expression.h` (linear atoms over places,
  booleans), `CtlFormula.h` and `CtlSimplify.h` (CTL over those atoms),
  `Property.h` (a named property and its kind: reach, invariant, deadlock,
  bound, CTL), `Simplify.h`, `Hint.h` (Parikh hints), `SexprPrinter.h` (the
  tree as s-expressions, indices or names). Its `README.md` and
  `algorithm.md` are upstream's.
* `parse/mcc/` — the expat handler for the MCC property XML, CTL included.
* `parse/sexpr/` — the s-expression reader (`Sexpr.h`, itself a copy of our
  `surface/sexpr.hh`), `PropertyReader.h` (forms to properties: the grammar
  of PetriSpot's `INTEROP.md`), `HintReader.h`.
* `parse/NetResolver.h` (names and `p<i>`/`t<i>` indices against a net),
  `parse/PropertyFile.h` (syntax choice by extension).
* `io/SparseMatrixIO.h`, `io/PNETIO.h` — KERS matrices and the PNET binary
  net container.

Local edits (only these, applied by `vendor.sh`):

* `Rational.h`: added `#include <numeric>` (was transitively satisfied upstream).
* `Arithmetic.hpp`: `inline` on the `__uint128_t operator<<` (ODR across TUs;
  upstream is a single translation unit).
* `Log.h`: the log line goes to `stderr`, since stdout carries the emitted
  model (`nupn2hsc`) or the answer protocol.

The invariant *solver* (`InvariantMiddle`/`Calculator`/`Heuristic`/`RowSigns`),
the walk engine and the CTL checker are **not** vendored.

## Ours

* `nupn.hh` + `src/petri_nupn.cc` — the NUPN unit tree. `PTNetHandler` treats
  `toolspecific` as opaque, so a second SAX pass reads it; logic mirrors
  `fr.lip6.move.gal.nupn.NupnHandler`. A unit holding both places and subunits
  is kept as-is (bracketed together), not split into a synthetic child.
* `decompose.hh` + `src/petri_decompose.cc` — a unit tree for a net that
  arrives without one, by Louvain clustering over a place co-occurrence graph
  (control→write per transition, all-to-all as fallback). The graph is built
  from the net alone: no property takes part, so nothing can make one community
  mandatory. `louvain/hyperedge.hh` bounds what a single transition may
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
