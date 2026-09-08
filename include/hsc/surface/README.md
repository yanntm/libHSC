# `surface/` — a file surface for the calculus

Runs a model from a file instead of a `.cc`. The surface is an SMT-flavoured
s-expression syntax. Templates, parameters and instance arrays are resolved
by the surface's own parametric pass (`expand.hh`) before meaning is
assigned — what reaches the translator is small: integer leaves (Int by
type, a finite domain opt-in), a shape, and events with symbolic guards.

## Modules

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
  input; `(print-spec)` dumps its output as runnable flat `.hsc`.

* `translate.hh` + `src/surface_translate.cc` — **M2M: AST → operations.** A
  separate pass that walks the `datum` forms and drives a `core::manager`:
  declares leaves and a shape, compiles each event to a product term
  (`core::product`), and executes the commands. This is the *only* module that
  depends on the engine.

* `rewrite.hh` + `src/surface_rewrite.cc` — **the rewrite chain**:
  semantically neutral `datum → datum` passes with per-pass traces;
  `elide-constants` first (`algorithm.md` §1c). Reached from a file by a
  directive form (`(simplify-constants)`).

* `spec.hh` + `src/surface_spec.cc` — **declarations as data**: the
  binder-free spec read into a `name_scope` (leaves, arrays, frontier
  order, seeds, events), and the domain inference over it. No translator,
  no diagrams.

* `xpl_build.hh` + `src/surface_xpl.cc` — the bridge to the explicit
  engine: event forms → `xpl::model`.

* `src/surface_ctl.cc` — the CTL commands: `(ctl …)` reads a formula into
  the `hsc/ctl/` DAG (state subformulas as `(when …)` selectors,
  `(deadlock)` from the declared guards), runs the forward form over the
  default system from the seed; `(expect-ctl …)`, `(gfp …)`.
* `src/surface_run.cc` — **the runner**, the public `translate()` entry:
  applies the rewrite directives, routes explicit-engine commands
  (`xreach`, `xdomains`) and the query overlay on explicit results,
  hands every other form to the symbolic translator
  (`src/surface_translator.hh`, split by concern across
  `surface_translate/events/families/query.cc`). The one layer that
  knows both engines; neither engine knows the other.

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
