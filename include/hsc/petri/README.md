# `petri/` — PNML/NUPN import

The path from a Petri net file to the `.hsc` surface. It emits **text**: the
importer does not link the calculus core. Used by `tools/nupn2hsc`.

## Vendored, with attribution

These files are copied from **PetriSpot**
(<https://github.com/yanntm/PetriSpot>), © Yann Thierry-Mieg, GPL-3.0-or-later,
and kept close to source deliberately — solid, battle-tested code, and the
sparse-matrix representation is the substrate for any future endogenous
rewriting of the net (structural reduction, agglomeration) before export.

* `SparseArray.h`, `MatrixCol.h` — sparse column matrix (the flow matrices).
* `SparseBoolArray.h`, `Arithmetic.hpp`, `InvariantHelpers.h`, `Rational.h` —
  their support (overflow-checked arithmetic is deliberate robustness).
* `SparsePetriNet.h` — the net: names, marking, `flowPT`/`flowTP`.
* `PTNetHandler.h`, `PTNetLoader.h` — the expat SAX parser for PNML P/T nets.

Local edits (only these):

* `Rational.h`: added `#include <numeric>` (was transitively satisfied upstream).
* `Arithmetic.hpp`: `inline` on the `__uint128_t operator<<` (ODR across TUs).
* `PTNetLoader.h`: dropped the `InvariantMiddle` dependency (the invariant
  solver, not needed for import) and its two log calls; added `#include <chrono>`.

The invariant *solver* (`InvariantMiddle`/`Calculator`/`Heuristic`/`RowSigns`)
is **not** vendored.

## Ours

* `nupn.hh` + `src/petri_nupn.cc` — the NUPN unit tree. `PTNetHandler` treats
  `toolspecific` as opaque, so a second SAX pass reads it; logic mirrors
  `fr.lip6.move.gal.nupn.NupnHandler`. A unit holding both places and subunits
  is kept as-is (bracketed together), not split into a synthetic child.
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
