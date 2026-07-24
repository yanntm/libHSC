# `surface/` — a file surface for the calculus

Runs a model from a file instead of a `.cc`. The surface is an SMT-flavoured
s-expression syntax. Templates, parameters and instance arrays are resolved
by the surface's own parametric pass (`expand.hh`) before meaning is
assigned — what reaches the translator is small: integer leaves (Int by
type, a finite domain opt-in), a shape, and events with symbolic guards.

## Three modules, not one

The pipeline is split along the MDA seam, and the split is load-bearing: a
parser turns text into a tree, passes rewrite the tree, a translator gives
the tree meaning. Neither endpoint knows which passes intercede.

* `sexpr.hh` + `src/surface_parser.cc` — **T2M: text → AST.** A hand-written
  recursive reader for s-expressions. Its only job is nesting and tokens; it
  knows nothing of `hsc::core`. The AST is the s-expression tree itself (a
  `datum`): homoiconic, so no separate typed tree is minted here.

* `expand.hh` + `src/surface_expand.cc` — **the parametric pass, datum →
  datum.** `param`, count-form `array`, and the binders `forall` / `exists`,
  expanded by position through each context's native combinator (seq/and
  for `forall`, alt/or/event-family for `exists`). Identity on binder-free
  input; `hscrun --expand` dumps its output as runnable flat `.hsc`.

* `translate.hh` + `src/surface_translate.cc` — **M2M: AST → operations.** A
  separate pass that walks the `datum` forms and drives a `core::manager`:
  declares leaves and a shape, compiles each event to a product term
  (`core::product`), and executes the commands. This is the *only* module that
  depends on the engine.

The parser does not import the translator's headers and vice versa. A `datum`
carries a source line so the translator can report errors in the user's terms.

## Scope

Guards and actions are arbitrary expressions over the leaves, arrays
included. The compiler splits each event along the separable/crossing seam: separable
pieces become per-leaf products (the F/L saturation schedule), crossing
pieces become case brackets (`split_equiv` at the cut). In `select`
queries each atom is pinned to one leaf or relates two leaves
(`select_where` / `select_compare`); a predicate crossing more leaves than
that is expressed as a `(when …)` filter term and applied with `apply`.

See `algorithm.md` for the grammar, the parametric pass, and the compile
map.
