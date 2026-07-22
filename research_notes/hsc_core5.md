# Hierarchical Shape Calculus

*Working draft 5. Standalone: every object is defined from nothing.*

---

## 0. The stance

The object of study is the **classification of structured words**. A word is
a tuple bracketed by a tree; a classification assigns to each word a class;
the default classification is acceptance. An object *is* the equivalence
structure its classification induces on words. A representation is a tower of
quotients of that structure, one quotient per cut of the tree. Operations are
morphisms between classification spaces. Canonicity is uniqueness of the
quotient relative to a stated classifier.

The ambition, stated once: **hierarchy over arbitrary sorts mixed freely,
with one mechanism general enough to mediate operations between the algebras
of the parts.**

Three layers, strictly ordered by citizenship. **Codes** are the citizens:
finite, canonical, hashable; every object a computation touches is a code.
**Classes** are what codes mean relative to a classified object and a cut.
**Elements** of the underlying carriers are the extensional shadow — they fix
what equality of behaviour means, and are never consulted to compute
anything.

Two constants are never citizens. `0` is *adjoined*: carriers are extended by
a free point, so `0` is a legal answer and never a legal letter. `id` is
*free*: the empty composition, never represented. Extending pointed
definitions by adjoined elements, instead of admitting those elements into
the definitions, is the single move that eliminates the traditional
degenerate cases — and, as §4 shows, it is also what makes skipping cost
nothing.

The claimed contribution is one structural identification, from which the
rest follows:

> **Operations are shape-indexed in the same way data is.** The term algebra
> at a composite sort contains the product of the term algebras at its parts.
> Skipping, position-independent naming, and the saturation partition are not
> three mechanisms; they are three readings of that one sentence.

Deliberately out of scope: weighted and multi-terminal readings; inverse
images, absolute or relative; and the choice of shape, which §8 argues is not
a question this calculus should be asked.

---

## 1. Shapes and words

**Definition 1.1.** Fix a family of imported theories. **Shapes** are

```
V ::= 1 | ⟨A⟩ | (V_h , V_t)
```

with `⟦1⟧ = {•}`, `⟦⟨A⟩⟧ = U_A`, `⟦(V_h,V_t)⟧ = ⟦V_h⟧ × ⟦V_t⟧`. A **word**
of sort `V` is an element of `⟦V⟧`. The **frontier** of `V` is its sequence
of leaves, left to right; it exhibits words as tuples, bracketed statically
by the tree. A **position** is a path from the root.

Three commitments, each load-bearing later:

- **No names.** Positions are paths; ordered trees are rigid; two equal
  subterms are one object. Naming is surface decoration, erased at
  declaration.
- **No associativity.** `(V₁,(V₂,V₃))` and `((V₁,V₂),V₃)` are distinct sorts
  with a computed isomorphism between them. Refusing that quotient is what
  *hierarchical* means here.
- **The unit sort is mono-sorted.** `1` has no frontier; no position
  addresses it.

**Definition 1.2.** At `1` sit, as one object: the terminal value, the
default classification, and the base case of the default classifier.

---

## 2. Theories: a six-map interface

A leaf imports an external domain through a finite window. The window, not
the domain, is what the calculus sees. The window is smaller than one might
expect, and pinning down exactly how small is the first result.

**Definition 2.1 (support algebra).** A family `ℛ ⊆ 2^U` containing `∅`, with
canonical finite codes for its elements, closed under finite union, finite
meet, and **relative** difference `A ∖ B`. This is a generalised Boolean
algebra: relatively complemented, distributive, least element, **no top
required**.

**Definition 2.2 (local terms).** A theory exports a term algebra `𝒯` with a
distinguished `id`, an action `· : 𝒯 × ℛ → ℛ`, a sum `+ : 𝒯² → 𝒯`, and a
reflexive closure `(−)^* : 𝒯 → 𝒯` satisfying

```
id · A = A            (t + u) · A = t·A ∪ u·A            t^* · A = ⋃_{n≥0} tⁿ·A
```

The theory is handed a *maximal* term — the whole thing, guards, updates,
composition, sum, closure — and interprets it however it likes. Splitting a
code, acting per piece, and re-joining is what the calculus must never do to
a theory that has a native operation.

**Definition 2.3 (exported maps).** A theory may export maps `f : U → U'` by
exporting the pushforward `f_* : ℛ → ℛ'`. **No preimage is exported,
absolute or relative, and none is ever requested.**

**Proposition 2.4 (minimality of the interface).** A sort's entire
obligation to the calculus is six maps: `∪`, `∩`, `∖` on codes, and `·`, `+`,
`(−)^*` on terms.

*Why the usual suspects are absent.* **Equality** and **emptiness** appear in
every axiomatisation of such an interface and are not in this list. They are
discharged by canonicity rather than implemented: two codes denote the same
class iff they are the same code, and the empty class is the adjoined `0`,
which is an absence rather than a value. A theory is never asked either
question. **Complement** is absent because no construction below forms one;
relative difference is the only negation the calculus can express, and §4.6
shows it is enough. **Preimage** is absent by Definition 2.3.

**Definition 2.5 (currification).** A theory's codes are relative to a
position, not absolute: one indexing is ever instantiated, and every position
using that theory uses the same one. A leaf of width `k` uses the first `k`
names. Consequently *a code built for one position is the code for every
other position with the same local structure*.

The price is precise and is the origin of §7: a criterion relating
coordinates in two different positions cannot be a single object of the
theory, because both would use the same names.

**Definition 2.6 (interchange theory).** A criterion whose subterms are
evaluated in different theories needs a sort in which those theories agree;
that sort is declared, and the theory over it is the interchange theory.
Equality is always available; linear integer arithmetic is the working one.
Types are declared; alphabets are discovered.

**Obligation 2.7 (finite orbits).** For `t^*` to denote, the orbit
`{tⁿ·A}_{n≥0}` must be finite for every code `A`. This is not implied by
finiteness of `ℛ`'s codes: a shift on an unbounded integer domain has finite
codes and infinite orbits. A theory owes either a declaration that its terms
have finite orbits, or a bound. Stating this is not pedantry — it is the one
obligation whose omission turns a closure into a non-terminating computation
rather than a wrong answer.

---

## 3. Diagrams

**Theorem 3.1 (canonical decomposition).** Let `X` be a nonzero
classification at sort `(V_h,V_t)`. There is exactly one expression

```
X = Σ_{i=1..n} P_i ⊗ S_i
```

with the `P_i ∈ ℛ(V_h)` nonempty and pairwise **disjoint** (D), and the `S_i`
nonzero and pairwise **distinct** (F): the `S_i` are the distinct nonzero
sections `X(x,–)`, and the `P_i` are their fibres.

**Reading 3.2.** Define `x ~ x'` iff `X(x,–) = X(x',–)`. Theorem 3.1 says the
primes are the classes of this continuation congruence, constructed through
meets and differences; the subs are the class *names*, compared by code
equality. Dually `X` induces a congruence on the tail coordinate, which this
node does not materialise — the subs' own decompositions do, one cut at a
time. **A representation is the classification's congruence tower,
materialised one cut per node and deferred elsewhere.**

**Definition 3.3 (diagrams).** `Diag(1) = {0, 1}`; `Diag(⟨A⟩) = ℛ(A)` with
`0 = ∅`; `Diag((V_h,V_t))` is the adjoined `0` or a finite nonempty set of
pairs `{(P_i,S_i)}` satisfying (D) and (F). Arcs draw from the unpointed
carrier; `0` enters only as an answer.

**Construction 3.4 (the canonicalizer).** Write `Bag(V)` for finite multisets
of *rectangles* `(P,S) ∈ ℛ(V_h) × Diag(V_t)`. Define

```
canon : Bag(V) → Diag(V)
```

by incremental insertion into a maintained partition: for each rectangle,
meet it against the cells present, carve overlaps out by relative difference,
add sections where heads overlap, and group by sub. The cell whose expression
would require a top — the all-negative pattern — has section `0` and is
deleted by the pointed discipline before it is written. Every prime ever
written is a nonempty meet or a nonempty relative difference of primes
already present, which is exactly the invariant that maintains (D).

**Proposition 3.5 (universality).** Every operation whose codomain is
`Diag(V)` factors through `canon`. Union, meet, relative difference, and the
action of every operation term of §4 all present their result as a bag and
canonicalise it.

This is worth stating because it is what makes the calculus small: there is
one place where a diagram is made, one place where (D) and (F) are
established, and one place to look when either fails.

**Definition 3.6 (disjoint bags).** A bag is *pre-disjoint* if its heads are
already pairwise disjoint.

**Proposition 3.7.** On a pre-disjoint bag, `canon` reduces to grouping by
sub: no carving occurs. Meet, relative difference, and the action of a term
whose head component is a **selector** (§4.6) all produce pre-disjoint bags.
Union does not.

*Proof sketch.* For meet, the heads are `P_i ∩ Q_j` for two partitions, and
distinct `(i,j)` give disjoint results. For difference, each `P_i` is
partitioned among the `Q_j` plus a remainder. For a selector `s`, the heads
are `s·P_i ⊆ P_i`, and subsets of disjoint sets are disjoint. ∎

This is a statement about **cost, not meaning** — both paths compute the same
normal form. It belongs in the calculus anyway, because it identifies which
operations can be given a cheaper universal property, and because the
criterion turns out to be exactly *selector-hood*, which §4.6 defines for
independent reasons.

**Theorem 3.8 (internalisation).** For any shape whose head-position subtrees
are hereditarily tier G, `Diag(V)` equipped with `∪, ∩, ∖` and with the term
algebra of §4 satisfies Definition 2.1 and Definition 2.2. Hence `⟨Diag(V)⟩`
is a legal import: **the calculus eats its own output, and *hierarchical* is
this closure property.**

There is one diagram type, not two. A prime over a composite head *is* a
diagram over that head. The induced typing law: head subtrees uniformly tier
G, tail spines pay as they go — *classes need differences; names need
equality.*

---

## 4. Operations are shape-indexed

This is the section the rest of the paper turns on.

**Definition 4.1 (the term algebra).** For each sort `V` define `𝒯(V)` by
structural recursion:

```
𝒯(1)          = {id}
𝒯(⟨A⟩)        = the theory's own term algebra (Definition 2.2)
𝒯((V_h,V_t))  ⊇ 𝒯(V_h) ⊗ 𝒯(V_t)
```

closed in every case under `+`, `∘`, and `(−)^*`, and — at composite sorts
only — under the query/case bracket of §7. The **product term** `h ⊗ t` acts
by

```
(h ⊗ t) · ( Σ_i P_i ⊗ S_i )  =  canon { ( h·P_i , t·S_i ) }_i
```

and `id_{(V_h,V_t)} = id ⊗ id`.

Two things to notice, and they are the same thing seen twice. First, `𝒯` is
defined by recursion on the shape, in exact parallel with `Diag`. Second, the
recursion at a composite sort is *not* over positions of the frontier but
over the two subtrees; a term never names a leaf.

**Proposition 4.2 (the parallel).** `Diag((V_h,V_t))` is built from
`ℛ(V_h) × Diag(V_t)`; `𝒯((V_h,V_t))` is built from `𝒯(V_h) × 𝒯(V_t)`.
Operations have the shape of the data they act on.

**Theorem 4.3 (skipping is free).** `(id ⊗ t) · X` does not inspect the heads
of `X`, and its result has the same heads.

*Proof.* By Definition 4.1 the bag is `{(P_i, t·S_i)}`. The heads are
unchanged, hence still pairwise disjoint, hence by Proposition 3.7 no carving
occurs and no operation of `ℛ(V_h)` is invoked. ∎

The classical treatment introduces a *skip predicate* — an oracle answering
"does this operation touch this level?" — computed from a support set,
memoised per level, and consulted at every node. Theorem 4.3 removes the
question rather than answering it faster: `id` is free, free things are not
represented, and a term that transmits `id` to a subtree transmits nothing.
There is no predicate to compute and nothing to memoise. In particular
`id ⊗ id` collapses to `id`, so a term that touches one leaf of a large shape
is a path of that shape's depth and nothing else.

**Theorem 4.4 (currification of operations).** `𝒯(V)` depends on `V` alone.
Hence if two positions of a shape carry isomorphic subtrees, their term
algebras are equal — not isomorphic, equal — and a term written for one *is*
the term for the other.

*Proof.* Immediate from Definition 4.1: the recursion mentions no position. ∎

This is the operation-side counterpart of Definition 2.5, and it is the
reason the calculus can be currified at all. A presentation in which an
operation names an absolute coordinate cannot have Theorem 4.4: two
structurally identical events at different positions are then different
objects, share no codes, and share no computation. The obligation this places
on any presentation of the calculus: **no term may name a position.**

**4.5 Filters and updates are not generators.** They are primitive *inside* a
leaf term, and derived across a cut: §6 shows how far products and sums get,
and §7 takes over exactly where they stop.

**Definition 4.6 (the two faces, and selectors).** A support element `e`
types two ways: as a *point* ("these words") and as a *sub-identity* ("keep
these words"). One code, two typings; the second carries `∩` to `∘` and `∪`
to `+`. A term is a **selector** if `t·A ⊆ A` for all `A`.

**Theorem 4.7 (the guard algebra).** The sub-identity interval `[0, id_V]` is
a unital Boolean algebra even over non-unital supports: its top is `id`,
which is free, and complement is computed on data as a relative difference,
`(¬h)(w) = {w}` iff `h(w) = 0`. No top is needed anywhere.

The morphism algebra therefore strictly outruns the data algebra: a derived
predicate such as *deadlocked* — the complement of the union of all enabling
guards — is computable against the data present, with no `⊤` in sight.

**Proposition 4.8.** The operator semiring is zerosumfree
(`h + h' = 0 ⟹ h = h' = 0`, hence monotone fixpoints) but not
zero-divisor-free (disjoint guards compose to `0` with both nonzero).
Negation, banished from the semiring, survives exactly on the idempotent
interval, where the free unit gives it something to be relative to.

**4.9 Deadlock is exact.** A class pair contributing nothing contributes the
empty sum, a value of the same construction that produces every other. No run
is ever an under-approximation on account of a junked class.

---

## 5. Closure, and saturation as an identity

**Definition 5.1.** `H^*` is the least fixed point of `X ↦ X ∪ H·X`;
equivalently `Σ_{n≥0} Hⁿ`. It is reflexive by construction.

Consider a closure at a composite sort whose operand is a sum of events. By
Definition 4.1 each product-shaped event is `h ⊗ t`, and reading the pair
partitions the sum with no analysis whatever:

```
F = Σ_j (id ⊗ f_j)     reaches the tail only
L = Σ_k (l_k ⊗ id)     reaches the head only
G = the remainder      reaches both
```

Write `𝐅 = id ⊗ (Σ_j f_j)^*` and `𝐋 = (Σ_k l_k)^* ⊗ id`. Both are terms at
this sort whose components are *closures one level down*.

**Theorem 5.2 (saturation).** With `H = id + F + L + G`,

```
H^*  =  ( 𝐅 ∘ 𝐋 ∘ (id + g_1) ∘ ⋯ ∘ (id + g_m) )^*
```

for any enumeration `g_1,…,g_m` of `G`.

*Proof.* Write `K` for the operand on the right. Every factor of `K` is `≥
id` and `≤ H^*`; since `H^*` is reflexive and transitively closed it is
closed under composition, so `K ≤ H^*` and hence `K^* ≤ H^*`. Conversely,
because each factor is `≥ id`, each factor is `≤ K`; in particular `F ≤ 𝐅 ≤
K`, `L ≤ 𝐋 ≤ K`, and each `g_i ≤ K`, so `H ≤ K` and `H^* ≤ K^*`. ∎

Three readings, and the third is the point.

**The identity is unique; the schedule is not.** The proof used nothing about
the order of the factors. Every ordering — and every interleaving that keeps
each factor `≥ id` — yields the same closure. Ordering is therefore a purely
extensional degree of freedom: it moves the bill and cannot move the meaning.
Where implementations of this schedule differ from one another — re-applying
the local closure after each crossing event, or closing each `g_i`
individually before moving on — they are choosing among instances of Theorem
5.2, and no such choice needs a soundness argument. That is a much better
account than treating one arrangement as canonical.

**Hierarchy is automatic.** `(Σ_j f_j)^*` is a closure at `V_t` and
`(Σ_k l_k)^*` a closure at `V_h`, so Theorem 5.2 applies again, one level in.
The recursion bottoms out where a theory closes its own local term
(Definition 2.2). No annotation, no user-supplied locality, no declared
decomposition — the split is *read off* the product structure of the terms.

**Why the saturated form is cheaper.** Both sides of Theorem 5.2 denote the
same operator, so the difference is entirely in evaluation. Under
memoisation keyed by `(term, argument)`, the right-hand side keys on
*subterms applied to nodes*: `𝐅` meets the same sub-diagram repeatedly across
the computation and answers immediately after the first time. Round-based
iteration of the left-hand side applies `H` to a *fresh* argument each round,
so the key differs each round and the memo never fires. The customary
slogan — a node is saturated once — is precisely the statement that the
closures sit inside the term rather than outside it.

**Proposition 5.3 (soundness has a cheap witness).** Theorem 5.2 is an
identity between operators on canonical objects. Since diagrams are
canonical, agreement of the two sides on an argument is decidable by code
equality. A saturating implementation therefore admits an exact differential
oracle against naive iteration, at any sort, on any model.

---

## 6. What products and sums already reach

Definition 4.1 gives product terms and sums. It is worth knowing precisely
how much of ordinary modelling that is, because the answer is: most of it,
and the boundary is not where one expects.

**Definition 6.1.** A criterion `φ ⊆ ⟦V⟧` is **separable** if it lies in the
distributive lattice generated by criteria each of which constrains a single
position of the frontier. An update `u : ⟦V⟧ → ⟦V⟧` is separable if it is a
product of per-position maps.

**Theorem 6.2 (the query-free fragment).** Separable criteria and separable
updates are exactly those expressible by product terms and sums.

*Proof sketch.* A single-position criterion `φ_ℓ` is the product term with
`φ_ℓ` at `ℓ` and `id` elsewhere. Conjunction is the product term carrying all
the components at once — one traversal, not a composition. Disjunction is the
sum of the corresponding product terms: `(φ_1 ⊗ id) + (id ⊗ φ_2)` selects
exactly the words satisfying either. Products and sums generate the
distributive lattice, and conversely a product term constrains each position
independently by construction. ∎

**Corollary 6.3.** A criterion whose *support spans a cut* need not be
crossing in any expensive sense. The condition "no smaller ring sits on
either of these two poles", though it mentions every position below a cut, is
a conjunction of per-position conditions and is a single product term. So is
a Petri transition, whose guard and update are per-place. So is mutual
exclusion, once the shared resource is given its own position rather than
being encoded implicitly into its neighbours' states.

This corrects a natural but wrong reading of Definition 2.5. The obstruction
there is *not* that a criterion mentions several positions; it is that a
criterion may **fail to factor** into per-position parts. Support crossing a
cut is cheap. Non-separability is what costs, and §7 is about that and only
that.

**Proposition 6.4.** The separable fragment is closed under the saturation of
Theorem 5.2: if every event is a product term, then `F`, `L` and `G` are, and
so are `𝐅` and `𝐋`.

---

## 7. Queries and cases: the inseparable fragment

What remains is exactly the criteria and updates outside Definition 6.1:
`a < b + c` relating positions across a cut, `x ≔ y + z`, `tab[i] ≔ e`. These
cannot be product terms, because by Definition 2.5 both ends would name the
same coordinates.

The construct is a **query/case bracket**: at a cut, a query partitions the
head by the *residual* of a criterion — what remains to be decided once the
head's contribution is known — and the case dispatches on the residual,
sending each class a continuation into the tail. The residual is a code of
the interchange theory (Definition 2.6); the calculus never interprets it,
but the theories at the two ends must interpret it identically, and that
agreement is what declaring the interchange theory buys.

**The partition primitive.** A theory must export `split_equiv`: partition a
code by the residual of a criterion, returning the realised classes indexed
by residual. Enumerating the carrier is a legal implementation and the honest
one for a small finite carrier; a theory earns its keep by returning few
structured codes where enumeration would return many singletons.

**Proposition 7.1.** By Theorem 3.8 diagrams are a theory; therefore diagrams
must export `split_equiv`. There is no leaf case and node case — one
construct, whose leaf instance is delegated and whose node instance is the
bracket itself, one level in.

**The crossing event under saturation.** An event in `G` — one that reaches
both sides of a cut — is a case, and re-saturation is fused into its reply,
because the reply is a morphism and morphisms compose. Whether the event
factors as `l ∘ f` survives only as a **cost regime**, never as a case
distinction in the semantics.

This section is the part of the calculus that remains to be built, and §6 is
the reason that is tolerable: the boundary sits far enough out that a
substantial class of models never reaches it.

---

## 8. The object is shape-free; the representation is not

`⟦V⟧` depends on the frontier of `V`, not on its bracketing. Two shapes with
the same frontier denote the same words and admit a computed isomorphism.
`Diag(V)` does *not* depend only on the frontier.

**Proposition 8.1.** There are families of classifications and pairs of
shapes `V, V'` over the same frontier for which `|Diag(V)|` is `O(log n)` and
`|Diag(V')|` is `Θ(n)` — and other families for which the same two shapes
exchange roles.

For a ring of `n` identical components whose interaction is nearest-neighbour
and whose cuts are each crossed by a bounded number of constraints, a
balanced shape shares every segment of equal length and the representation
does not grow with `n`; a spine pays one level per component. For a system
whose events reach a *suffix* of the frontier, the spine makes exactly those
events skippable in the sense of Theorem 4.3 while a balanced shape cuts
across every one of them, sending each into `G` at every level.

Neither is a defect. Both are the same object under different presentations,
and the presentations differ in cost by margins that grow with `n` in both
directions. It follows that **the shape is a parameter of the representation,
supplied from outside**, and choosing it well is a question about a model's
dependency structure — a community-detection problem over a graph in which
control dependencies and data dependencies are distinguished — rather than a
question about this calculus. The calculus consumes a shape and refuses to
guess one; the honest statement of its scope includes saying so.

---

## 9. Degenerations

The calculus specialises rather than competing with its special cases. Each
boundary below is recognisable from the syntax alone, and specialising across
it must cost nothing.

**Flat.** Take `V` a right spine of leaves, each `⟨A⟩` with `A` a finite
enumerated domain, and every term separable (Theorem 6.2). Then primes are
sets of values, `canon` is the classical union of decision-diagram nodes, the
partition of §5 is the classical per-level split, and Theorem 5.2 is the
classical saturation schedule. The classical flat calculus is *the value of
this calculus at the spine*, not a competitor to it.

**Single leaf.** Take `V = ⟨A⟩`. Diagrams are theory codes, terms are theory
terms, and the calculus disappears into the theory — as it should.

**Query-free.** Take every term separable. §7 never fires; the operation
basis is `id`, products, sums, composition, closure. Saturation is unaffected
(Proposition 6.4). This is the fragment in which the entire structure of §§3–6
is already exercised, which is why it is a sensible place to stop and check.

**No hierarchy in the heads.** Take every head position to be a leaf. Then
Theorem 3.8 is never used and the tower is one level deep in the sense that
matters, though the spine may be long.

---

## 10. Open

1. **The inseparable fragment (§7).** The bracket, its contract, its
   residual normalisation, and its interaction with Theorem 5.2 when the
   reply does not factor.
2. **A syntactic criterion for pre-disjointness beyond selectors.**
   Proposition 3.7 gives selectors; the general condition is that the head
   component maps disjoint codes to disjoint codes, which is a property of a
   theory's terms that a theory could declare.
3. **Finite orbits (Obligation 2.7).** What a theory should be required to
   declare, and whether the calculus can discharge it structurally for the
   fragment of §6.
4. **Cost regimes.** Theorem 5.2 leaves the schedule free. Which instance to
   choose is an extensional-free parameter, and the calculus should be able
   to *say* what distinguishes them without pretending one is canonical.
5. **Isomorphism between shapes.** §8 asserts a computed isomorphism between
   shapes over a common frontier. Its cost, and whether re-shaping a
   representation in place is ever cheaper than rebuilding it, is open.
