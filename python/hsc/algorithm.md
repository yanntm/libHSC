# hsc — the shape of the system

Overview and index. Each package documents its own algorithms; this file
says what the packages are, what crosses between them, and which invariants
every package is obliged to preserve. Section marks `§` refer to
`research_notes/hsc_core.md`.

Written before the code, and kept ahead of it: the code is meant to read as
a transcription of these documents, not the other way round.

---

## The four packages

| package | owns | algorithms |
|---|---|---|
| `hsc/core` | shapes, diagrams, the canonical decomposition, the support algebra | `hsc/core/algorithm.md` |
| `hsc/classify` | the classifier language and `split_equiv`, the traversal backbone | `hsc/classify/algorithm.md` |
| `hsc/ops` | homomorphisms: local, combinators, classifier-driven | `hsc/ops/algorithm.md` |
| `hsc/leaves` | imported support algebras, one module per theory | `hsc/leaves/algorithm.md` |

The dependency order is exactly that order. `core` knows nothing of
classifiers or operations; `classify` knows `core`; `ops` knows both;
`leaves` implements an interface declared in `core` and is otherwise
independent of everything above it.

---

## `split_equiv` is the backbone

One traversal, and every construct that inspects data is a caller of it,
differing only in the codomain of the classifier and in what it does with
the returned pieces:

| caller | classifier codomain | action on the pieces |
|---|---|---|
| single-coordinate guard | binary | keep the accepting piece; does not travel |
| multi-coordinate guard | binary | keep the accepting piece |
| `assign(x := e)` | a value sort | write the constant on each piece |
| `Θ` | any declared sort | keep all pieces, labelled |
| `case` | any declared sort | apply the branch for that letter |
| the default classification | the unit sort | one piece: acceptance |

This is not a layering choice made for convenience. §9 says the structural
normal form is the special case of the quotient operator whose classifier is
the tautological one; building the quotient operator *first* and deriving
the rest from it is that statement, in code.

---

## Invariants that cross package boundaries

Every package is obliged to preserve these, and none may re-derive them.

1. **Zero is absence.** `None` at every shape. Never stored inside a node,
   never traversed, never sorted, never a letter. Emptiness at a composite
   shape is therefore a pointer test.
2. **Canonical means interned.** Equal diagrams are the same object.
   Anything that constructs a diagram goes through `normalize`, so
   well-definedness is *restored* after each action rather than assumed to
   be preserved by it.
3. **One diagram type.** A prime over a composite head is a diagram over
   that head. Internalization is the class hierarchy, not a feature and not
   a later test.
4. **No top, no complement, no preimage.** Every carving is a meet or a
   *relative* difference against a prime already present. There is no
   inverse image in the interface, absolute or relative, and none is
   planned: undoing a destructive assignment needs a reachable set to move
   within, which is a different problem for a different iteration.
5. **Values live in classifiers, not in terminals.** The unit sort carries
   acceptance and nothing more. Counting, weighting, provenance,
   edge-valued and multi-terminal readings are all classifier codomains,
   discovered per query. The calculus does not grow a terminal algebra to
   express what a classifier already expresses.
6. **Normalisation strength is a cost parameter.** A leaf that fails to
   detect an equality of terms causes redundant subqueries, which the
   kernel merge recovers on the way back up. It never causes a wrong
   answer, and no code path may assume otherwise.

---

## Measurement is a product, not telemetry

The cost model is something the prototype must *measure*. Counters live in
`core/stats.py` from the first commit, named `<sort>.<op>` and separated by
side of the leaf interface — at composite shapes most of the work is the
kernel's own recursion, and a bill reporting only leaf traffic under-reports
it. `classify/query.py` adds partition and kernel counters. No claim about
cost is made in this repository without citing them.

---

## What is deliberately absent

- **A schedule.** `Star` is plain BFS; its cost is rounds x events x |X| in
  freshly built nodes. Hierarchical saturation is now *specified* in
  `hsc/ops/algorithm.md` §5 and not implemented: the split of a term into
  local, crossing and skipping parts is derived from supports, a Kronecker
  crossing event is thrown both ways with its re-saturation fused in, and
  the rest is chained. What stays open there is the footprint notion — a
  static support over-approximates as soon as an operation reads an index it
  computes — and the non-Kronecker crossing case, which is exactly where
  `split_equiv` fires.
- **A term normal form.** The shape of the answer is a confluent rewriting
  system, not a canonical guarded word; see `hsc/ops/algorithm.md`.
- **An interchange syntax.** Models are built in Python until the classifier
  seam stops moving.
