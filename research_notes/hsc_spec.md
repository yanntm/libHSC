# HSC — Implementation Direction

*Companion to `hsc_core.md`. Despite the filename this is not a
specification: it is a line of implementation — architecture, concrete
representations, and ordered milestones with acceptance gates. Section marks
`§` point into the calculus document. Target language: Python. Performance
is not a goal; the products are the API, the measured cost model, and the
falsification of the document's claims.*

*Revised after the bootstrap iteration. What that iteration established is
recorded in `hsc/` — `algorithm.md` at the root for the invariants, one per
package for the algorithms — and is not restated here.*

---

## 0. Orientation

The three disciplines are implementation decisions already:

| discipline | in code |
|---|---|
| `0` adjoined, never a citizen | the null child is `None`; no node stores, iterates or hashes it |
| `id` free | the no-op is not an object; empty compositions vanish at construction |
| naming window | hash-consing; equality is a pointer test |
| partition window | `split_equiv` over leaf codes |
| per-run certificates | memo tables, residual-novelty checks, step counters |

**The backbone is `split_equiv`.** This is the central revision to this
document. The bootstrap iteration was written expecting a classical
decision-diagram kernel with the quotient constructor added on top as the
research contribution; that ordering is wrong. Every construct that inspects
data — a guard over more than one coordinate, a computed assign, `case`, `Θ`
itself, and the default classification — is one traversal with a different
classifier codomain and a different action on the returned pieces. Build the
traversal first and derive the rest; anything else duplicates it.

That is not an engineering convenience. §9 states that the structural normal
form is the special case of the quotient operator whose classifier is the
tautological one. Making the quotient operator the primitive is that
sentence, in code.

---

## 1. Core representations

**Shapes.** Interned trees, `1 | ⟨A⟩ | (V_h,V_t)`. Positions are frontier
paths.

**Diagrams.** One type, not two. A node is a frozen, canonically sorted
tuple of `(prime, sub)` pairs, unique-tabled. A prime over a composite head
is *a diagram over that head*: internalization (§6.4) is the class
hierarchy, available from the first line, not a milestone. Equality is `is`;
zero is absence.

**The unit sort** carries three roles as one object: the terminal value, the
default classification (acceptance), and the base case of the default
classifier (the continuation). Nothing richer lives there — see §5.

**The one central function.** `normalize(rectangles) -> node | None`,
Theorem 5.1 operationally. Against a naive reading of Construction 5.3: do
**not** enumerate sign-pattern cells; insert rectangles incrementally into a
maintained disjoint partition, splitting via leaf meet/relative difference
and merging equal subs by pointer. Same canonical result, apply-style
complexity. `normalize`, `join` and `diff` are mutually recursive; the memo
tables hang off that recursion.

---

## 2. The leaf interface

One ABC per tier ledger; a leaf declares its tier and implements up to it.
Tier G — meets and **relative** differences, **no top** — is the working
tier. Rules of the house:

- unimplemented tiers raise, so the interface ledger is a runtime discipline
  and the exception names the theorem being violated;
- **there is no preimage, absolute or relative, and none is planned.** The
  earlier draft of this document priced a relative preimage at tier B and
  made `Θ` optionally depend on it. Both are withdrawn. `split_equiv`
  supersedes it, and backward reasoning needs a reachable set to move
  within, which is a different problem for a different document;
- every leaf method is call-counted; the bill is a product, not telemetry.

**`split_equiv(code, expr)` is the leaf's real interface.** Partition a code
by the residual of a classifier after substituting this coordinate.
Enumerating the carrier is legal and is what a finite enum leaf does; a leaf
earns its keep by returning few structured codes where enumeration would
return many singletons. That ratio is the third cost factor and it is the
one factor decided entirely by the leaf.

Classifier normalisation is a **cost parameter, never a soundness one**: an
equality a leaf misses costs redundant subqueries, which the kernel merge
recovers on the way back up. This is now measured, not assumed.

---

## 3. `split_equiv` — the traversal

Three movements.

**Down.** Travel to the first coordinate the classifier mentions; a
classifier mentioning nothing in a subtree returns there in one entry
without descending. Locality is the distance-zero case and it is free.

**Across.** Meeting a coordinate substitutes its class and renormalises, so
the travelling classifier stays ground and mentions only coordinates not yet
consumed. Because expressions are interned, grouping head-classes by
residual code and keying the memo table on that code are the same act.

Normalisation must apply *while a residual curries*, not only when a client
writes an expression. An applied operation therefore carries the builder
that made it. Rebuilding through a generic constructor normalises at the
wrong time, which is to say never — this cost the bootstrap iteration a
silent factor of ten and is the single easiest thing to get wrong here.

**Up.** Results federate. A partition is a **kernel** — the canonical,
interned tuple of pieces — plus a **labelling** from residuals into it.
Subqueries whose residual codes differ but whose realised partitions agree
share one kernel and differ only by a finite table.

Every partition federates, including those that did not descend and those
with a single class. Not descending is a locality optimisation; not
federating is an error, since it would let equal partitions carry different
kernels and the merge would stop being an equivalence. One-class partitions
come in two kinds — labelled by a *value* (the terminal case; with value
`1`, acceptance) or by a *residual expression* (a suspended computation) —
and the kernel/labelling split is exactly what represents both without
conflating them.

Anticipated redesign: the classifier/leaf seam will be renegotiated. Plan
for it to change twice; keep it thin.

---

## 4. The shadow oracle

Definition 4.2 is the test architecture. Over tiny carriers a behaviour is a
brute-force dict from words to values, and every kernel operation is
property-tested against it. The scary components ship with an independent
judge.

An oracle nobody has falsified is not an oracle: a mutation gate that
deliberately breaks `normalize` — dropping (F), dropping (D) — must fail the
suite for every mutant. Beyond raw agreement, encode canonicity (order
insensitivity, so equal denotation gives pointer equality), the degeneracy
ledger on every constructed node, and minimality of `Θ`'s alphabet.

---

## 5. What is deliberately not built

Each of these was in the earlier draft as a milestone and is withdrawn, with
the reason, so that no later iteration re-adopts it by inertia.

**Weights and terminal algebras.** A coefficient semiring at the terminal is
off the line. The classifier is already the more abstract view of what
multi-terminal and edge-valued diagram families express with terminal and
edge decorations: counting, weighting and provenance are *classifier
codomains*, discovered per query. Do not grow a terminal algebra to express
what a classifier already expresses. (This also retires obligation §14(vi)
as stated; the qualitative/quantitative split it wanted to confirm shows up
instead as the value/residual split in labellings.)

**A parade of leaf theories.** Enum and integer domains are plenty for
everything this line needs to demonstrate. Interval, regular and BDD leaves
were sequenced as milestones on the strength of the theory document's
examples; they are not what the prototype is short of. One further theory
whose codes are *structured* rather than enumerated would make the third
cost factor real instead of nominal — that is the only leaf-side gap that
matters.

**The tautological Θ as an acceptance gate.** Checking that `Θ` against the
continuation reproduces `normalize` is an internal probe. It is also
vacuous unless the continuation classifier is computed rather than read off
the node, which makes it more expensive than it is informative.

**Inverse images.** See §2. Committed.

---

## 6. Open questions, both theory-first

These are the two places the implementation is currently blocked on the
theory, and neither is discussed in `hsc_core.md`.

**(a) The term normal form.** §7.2 canonicalises an elementary term to a
strictly alternating guarded word, merging adjacent filters by meets. That
merging is canonical and *anti-optimal* — two single-coordinate guards that
never travel become one two-coordinate guard that must — and the document's
framing over-assumes in ways it does not need to. The shape of the right
answer is a **confluent rewriting system** whose rules consult commutativity
and the *relative support* of operands to decide reordering. Independence of
supports is the notion partial-order reduction uses to justify not exploring
both interleavings of independent events; here it justifies commuting,
merging, or declining to merge two terms. Obligation §14(v) should be
restated in those terms.

**(b) The schedule.** Iterating a fixpoint by rounds rebuilds a fresh
diagram each time, so every round misses the caches and the cost is
rounds x events x |X| in freshly built nodes — measured, and it dominates
everything. Saturation is the known answer, and a shape tree admits an
a-priori grouping per cut that a flat variable order cannot express. Two
things must be settled first: the footprint notion (a *static* support
over-approximates as soon as an operation reads an index it computes,
whereas `skip` is minimal and may be dynamic), and the order within a cut
(tail before local level; guards before moves; termination of
re-saturation after a crossing event fires).

The two are one problem seen twice: the reordering a confluent rewriting
system enacts is the reordering a schedule needs.

---

## 7. Milestones, with gates

Ordered checkpoints; each gate is an executable fact.

- **M0 — shadow.** Brute-force behaviours over enum leaves; the property
  harness runs. *Gate:* a deliberately wrong `normalize` is caught. **Done.**
- **M1 — kernel.** Shapes, unique table, `normalize`, the support algebra,
  local homs, star; enum leaf. *Gate:* Ex1 end to end — four primes at every
  cut, deadlock reachable, size linear at 8/16 philosophers. **Done.**
- **M2 — backbone.** `split_equiv`, the kernel merge, and its callers.
  *Gate:* `#eaters` discovers `{0,1,2}` from an unbounded codomain; the
  residue classifier's bill is independent of the carrier's range. **Done.**
- **M3 — the schedule.** Saturation, after (b) is settled on paper. *Gate:*
  Ex1's fixpoint cost becomes a function of the representation rather than
  of the round count; two schedules agree on the value.
- **M4 — a structured leaf.** One theory whose `split_equiv` returns
  structured codes. *Gate:* the third cost factor separates from the
  enumeration baseline on a measured bill.
- **M5 — terms.** A confluent rewriting system per (a). *Gate:* terms that
  differ only by independent reordering normalise together, and the schedule
  of M3 is expressible as its rules.
- **M6 — syntax.** Reader, evaluator, `size`/`bill` introspection. *Gate:*
  a model expressed entirely in the surface, no Python.
- **M7 — a stranger's theory.** Someone not holding this document's context
  adds a leaf from the ABC docstring alone. *Gate:* they succeed without
  reading the kernel. The real test of algebra-agnosticism.

Sequencing: M3 before any further cost claim about fixpoints; (a) and (b)
resolved on paper before M3 and M5 respectively; M0 before everything.

---

## 8. Decisions taken here, so the code need not relitigate

- **Cross-coordinate assigns are compiled, not primitive.** They go through
  structural maps to co-locate, or a `case` on the read-classifier — which
  is what the calculus says anyway. Constant writes across a cut need
  neither and are a plain rebuild along paths.
- **Terms are not canonicalised, applications are.** §14(v) is open, and the
  `(term, data)` cache does not care.
- **Debug/release carries the theory.** Debug asserts the degeneracy ledger
  on every constructor; release assumes it, which is exactly Prop. 6.3's
  promise. A release failure debug would have caught is a
  coefficient-discipline violation, so the diagnosis is free.
- **A local hom at a cut is the same hom one level down.** It recurses by
  re-invoking itself, so the application cache is hit at every level. A hom
  recursing through a plain helper walks the diagram's tree unfolding rather
  than its DAG and discards the sharing that is the whole point of the
  representation.

---

## 9. Use cases this should grow toward

Ordered by how specifically they need *this* framework rather than a flat
decision diagram.

- **Compositional reachability and model checking.** Hierarchical systems
  where the shape mirrors the architecture and no global variable order
  exists. The native habitat.
- **Parameterised and infinite-state verification.** Queues, channels,
  token passing, with the hierarchy supplying the decomposition flat
  approaches lack.
- **Knowledge compilation over mixed domains**, past where bit-blasting
  stops.
- **Provenance and factorised query results.** The congruence tower is a
  factorised representation of a relation, with per-query `Θ` as its
  interface — and provenance is a classifier codomain, not a terminal
  algebra.
- **Program analysis.** Abstract domains as leaves; the diagram is the
  disjunctive completion.
- **Configuration and product lines.** "Discovered, not declared" is the
  difference between a configurator and a combinatorial declaration nobody
  maintains.

---

## 10. What the prototype is for

Three products, in order: an API a stranger can extend (M7 is the proof); a
measured cost model — the §9 bill as printed invoices, either confirming the
three factors or handing §14(iii) a counterexample, both outcomes valuable;
and pressure on the open obligations. `split_equiv`'s implementation is a
draft of the transport lemma's induction, and wherever the code needs a case
the lemma lacks, the document iterates. The prototype is an instrument
pointed at the theory, not a delivery.
