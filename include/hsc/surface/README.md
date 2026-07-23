# `surface/` — a file surface for the calculus

Runs a model from a file instead of a `.cc`. The surface is an SMT-flavoured
s-expression syntax; the input is **generated and trusted** (its complexity —
templates, parameters, instance arrays — is resolved upstream, exactly as GAL's
front end resolves them before the engine sees anything). What reaches here is
small: bounded scalar leaves, a shape, and separable events.

## Two modules, not one

The pipeline is split along the MDA seam, and the split is load-bearing: a
parser turns text into a tree, a translator gives that tree meaning. Future
passes (typing, desugaring, shape rewriting) intercede *between* them without
either module knowing.

* `sexpr.hh` + `src/surface_parser.cc` — **T2M: text → AST.** A hand-written
  recursive reader for s-expressions. Its only job is nesting and tokens; it
  knows nothing of `hsc::core`. The AST is the s-expression tree itself (a
  `datum`): homoiconic, so no separate typed tree is minted here.

* `translate.hh` + `src/surface_translate.cc` — **M2M: AST → operations.** A
  separate pass that walks the `datum` forms and drives a `core::manager`:
  declares leaves and a shape, compiles each event to a product term
  (`core::product`), and executes the commands. This is the *only* module that
  depends on the engine.

The parser does not import the translator's headers and vice versa. A `datum`
carries a source line so the translator can report errors in the user's terms.

## Scope

The separable Presburger fragment (§6) — "everything that does not need
`split_equiv`". A guard is a conjunction of per-leaf comparisons; an action is a
constant assign or a constant shift on one leaf. This is enough for Hanoi,
philosophers, and 1-safe Petri / NUPN nets.

In `select` queries over a stored result, an atom relating two leaves
(`(< a c)`, any comparator) is in scope: it is the first §7 case, resolved by
`split_equiv` at the cut separating the two positions (`hsc/query.hh`).

Refused, with a pointer to §7 (parsed and understood, then declined — a staged
feature, not a syntax error):

* an *event* atom relating two leaves, or arithmetic on the right-hand side
  (`(< a (+ b c))`),
* an action whose value reads another leaf (`(:= x (+ y z))`),
* arrays and `a[i]` dereference — that is the SMT *array* theory, not
  Presburger, and it is what forces `split_equiv`.

See `algorithm.md` for the grammar and the compile map.
