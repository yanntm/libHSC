# Hierarchical Shape Calculus

*Working draft 2. Standalone: every object is defined from nothing, and the
document owes nothing. It is written to be the seed of its own iteration — a
reader holding only this text should be able to continue the work. §14 lists
where to push.*

*What moved since draft 1. Operations are the centre now, not an appendix to
the representation: `skip` is promoted to a discipline, composition travels
as a block, saturation is derived rather than deferred, and `split_equiv` is
the backbone from which filters, assigns, `case` and the quotient
constructor are all obtained. Coefficient semirings and multi-terminal
values are **out of scope** — they are classifier codomains, and a theory
that is good at them is a leaf sort, not a terminal algebra. Inverse images
are **out of scope and committed against**.*

---

## 0. The stance

The object of study is the **classification of structured words**. A word is
a tuple shaped by a tree; a classification assigns to each word a class; the
default classification — the semantics when nothing else is said — is
**acceptance**: the value a word earns by reaching the terminal. An object
"is" the equivalence structure its classification induces on words; a
representation is a **tower of quotients** of that structure, one quotient
per cut of the shape; operations are morphisms between classification
spaces; and canonicity means **uniqueness of a quotient with respect to a
stated classifier**. The structural normal form is the special case where
the classifier is the tautological one; the general case is an operation of
the calculus itself (§7).

The ambition, stated once so the rest can be read against it: **hierarchy
over arbitrary sorts mixed freely, with one mechanism general enough to
mediate operations between the algebras of the parts.** That mechanism is
label-based synchronisation, and the labels are *discovered* rather than
declared (§8). Everything below is in service of that sentence.

**The ontology.** Three layers, strictly ordered by citizenship:

- **Codes** are the citizens: finite, canonical, hashable. Every object a
  computation touches is a code.
- **Classes** are what codes mean, *relative to a classified object and a
  cut*: a code at a leaf names an element of the leaf's support algebra; a
  prime at an internal node names an equivalence class of the object's
  congruence at that cut. Classes are ephemeral and per-object; codes are
  stable.
- **Elements** of the underlying carriers are the extensional shadow. They
  fix what equality of behaviours *means*; they are never consulted to
  *compute* anything. A word read at any level, without following links, is
  a word over that level's classes; there is no level at which raw elements
  are read.

This is **not** setoid mathematics. In a setoid the equivalence is ambient,
fixed and global, and every operation carries a once-and-for-all proof of
respect. Here the congruence is relative to the classified object and the
cut — it is *X's* continuation congruence, discovered, not the carrier's.
Operations are not required to respect it and mostly do not: a filter
slicing across a prime refines the partition, and that is normal life.
Well-definedness is not assumed; it is *restored*, by re-canonicalisation
after every action.

**Four disciplines** govern everything, and are stated before any definition
because every definition obeys them.

1. **Typing by adjunction.** Two constants are never citizens. `0` is
   *adjoined*: carriers are extended by a free point, so `0` is a legal
   *answer* and never a legal *letter* — nothing canonical stores,
   traverses, or points to it. `id` is *free*: the empty composition, never
   represented. Extending pointed definitions by adjoined elements, instead
   of admitting those elements into the definitions, is the single move that
   eliminates the traditional degenerate cases.

2. **Vision by staging.** Every rule accesses a sort through one of two
   windows: the *partition window* (construct equivalence classes: meets,
   relative differences, emptiness) or the *naming window* (compare
   canonical codes: equality, zero test). Recognition is staged — each node
   materialises exactly one congruence, its own cut's, and defers all others
   downward. Limited vision is not a restriction; it is the statement that
   the other congruences are other nodes' business.

3. **Skip.** *(New in this draft, and the operator-side twin of 2.)* Every
   operation has a **top**: the highest level its support reaches. Above its
   top, an operation is `id` and the level transmits it unchanged. Below and
   at its top, it acts. Skipping is not an optimisation applied to a
   finished semantics; it is what says that an operation is *about* certain
   levels and about no others, exactly as staging says a node is about one
   congruence and no others. Discipline 2 is where a node's ignorance is
   legitimate; discipline 3 is where an operation's is.

4. **Finiteness per element.** No carrier, alphabet or algebra is required
   finite. What is required is that each *represented class* admit finite
   canonical codes and that each *run* of an operation touch finitely many
   codes. Global guarantees are replaced by per-element and per-run
   certificates, discovered rather than declared.

**Lineage, stated plainly.** The structural normal form generalises the
compressed, trimmed decision-diagram forms over variable trees; "primes" and
"subs" are used deliberately in that tradition's sense, with the shape tree
in the role of the vtree. The continuation congruence is the classical right
congruence of word classification. The elementary layer is guarded-command
territory. The claimed delta: **currified naming**, under which a code is
relative to its position and therefore shared across isomorphic positions —
which no fixed global variable order can express, since there each variable
occurs at exactly one place (§2.6, §9); operations as blocks that travel by
`skip`, from which hierarchical saturation is *derived* rather than
retrofitted (§5, §6); and quotienting against foreign classifiers as a
first-class canonical operation, with discovered labels as the
synchronisation vocabulary between otherwise unrelated theories (§7, §8).

**Out of scope, deliberately.** Weighted and multi-terminal readings: a
classification whose codomain is a semiring is a *classifier*, and a theory
that represents such classifications compactly is a *leaf sort*. Nothing is
gained by making the terminal carry an algebra — one would never want to
exhibit the parts of such a classification anyway, but rather to act on a
partition with a distinguished action per class, which is precisely §8.
Inverse images, absolute or relative: backward reasoning needs a reachable
set to move within, which is a different problem.

---

## 1. Sorts and shapes

**Definition 1.1.** Fix a family of imported theories. **Shapes**:

```
V ::= 1 | ⟨A⟩ | (V_h , V_t)
```

with denotations `⟦1⟧ = {•}`, `⟦⟨A⟩⟧ = U_A`,
`⟦(V_h,V_t)⟧ = ⟦V_h⟧ × ⟦V_t⟧`. A **word** of sort `V` is an element of
`⟦V⟧`; the **frontier** of `V` (leaves, left to right) exhibits words as
tuples read in order, bracketed statically by the tree. A **position** is a
path from the root.

Three structural commitments:

- **No names.** Positions are paths; ordered trees are rigid; two equal
  subterms are one object under hashing. Naming, where needed, is a
  decoration consulted by exactly one construct. §2.6 pushes this into the
  theories themselves, where it becomes load-bearing.
- **No associativity.** `(V₁,(V₂,V₃))` and `((V₁,V₂),V₃)` are distinct sorts
  with a *computed* isomorphism between them. The quotient of shapes by
  associativity flattens every shape to a sequence of leaves; refusing that
  quotient is what "hierarchical" means here.
- **The unit sort is mono-sorted.** `1` exists at one shape only and has no
  frontier, so no position addresses it.

**Definition 1.2 (the unit sort carries three roles as one object).** At `1`
sit, and are the same thing:

- the **terminal value** `1`;
- the **default classification** — acceptance, the value a word earns by
  reaching the terminal;
- the base case of the **default classifier**, the continuation, whose
  residual once every coordinate is consumed *is* that value.

The third identity is why classifying against the continuation reproduces
the normal form (§7.7): the normal form is the special case of the quotient
operator whose classifier is the tautological one. Nothing richer lives at
the terminal; see §0, out of scope.

---
**Ex1 (philosophers) — statics.** `n` philosophers in a ring, `n` a power of
two. Fork `F_i` sits between philosopher `i` and `i+1`; philosopher `i` uses
`F_i` (left) and `F_{i−1}` (right). Each philosopher is one leaf carrying
its local state — the fork on its left, whether it holds that fork, whether
it is eating — and the shape is balanced over the philosophers, so **every
cut is crossed by exactly two forks**. The point of the example is that its
representations stay linear in the number of philosophers while its state
space is exponential.

---

## 2. Theories

A leaf imports an external domain through a finite window; the window, not
the domain, is what the calculus sees. We say *theory* rather than *leaf
algebra* because what is imported is not only a family of sets but an
operational vocabulary and its own normalisation.

**Definition 2.1 (support algebra).** A family `ℛ ⊆ 2^U` containing `∅`,
with canonical finite codes for its elements. Cumulative tiers:

- **E**: codes; decidable equality; decidable emptiness.
- **J**: E + finite joins.
- **G**: J + finite meets and **relative** differences `A ∖ B` — a
  generalised Boolean algebra: relatively complemented, distributive, least
  element, **no top required**.
- **B**: G + top and complement.

Tier G is the working tier. A theory without a top is legal and expected;
no construction below forms one.

**Definition 2.2 (exported maps).** A theory may export maps `f : U → U'` as
raw material for assigns, by exporting the pushforward `f_* : ℛ → ℛ'` on
codes. **No preimage is exported, absolute or relative, and none is ever
requested.** Assignment is memory-destructive; asking what a destroyed state
came from is a question about the ambient carrier, outside the object under
study, and the discipline is to refuse it rather than to price it.

**Definition 2.3 (local terms).** A theory is asked not for one map at a
time but for a **maximal local term**: the whole term — guards, assigns,
composition, sum, star closure (§4) — whose support lies in this leaf,
handed over for the theory to interpret however it likes. A relational
theory fuses the term into a single relation and applies it once; an
enumerated theory executes the parts. **The normal form for a local term is
per theory; the calculus delivers the term and does not prescribe the
fusion.** Splitting a code, acting per piece and re-joining is what a theory
with a native operation must never be made to do.

**Definition 2.4 (normalisation strength).** A theory exports canonical
codes not only for its support elements but for the closed terms of its
operation language — the expressions that travel as curried classifiers
(§7). The strength of this normalisation is a declared property, and a
**cost parameter, never a soundness parameter**: undetected equalities cause
redundant subqueries, which the merge of §7.5 cleans up retroactively; they
never cause wrong answers.

**Definition 2.5 (`split_equiv`).** The theory's partition primitive:
partition a code by the value of a local expression, returning the realised
classes with their labels. Contract and typing in §7.2. Enumerating the
carrier is a legal implementation and the honest one for a small finite
carrier; a theory earns its keep by returning few structured codes where
enumeration would return many singletons, and that ratio is the one cost
factor decided entirely by the theory.

**2.6 Currification.** A theory's codes are **relative to a position, not
absolute**. One set of variables — or one indexing, whatever the theory's
internal representation — is ever instantiated, indexed from the terminal
upward, and every position using that theory uses the same one. A leaf of
width `k` uses the first `k`.

The consequence is that *a code built for one position is the code for every
other position with the same local structure*. Four philosophers in the same
local state are one code, and the representation never learns that there are
four of them. This is discipline 1's "no names" pushed inside the theory:
positions are paths, and two equal subterms are one object.

It is also where this calculus departs from vtree-based decision forms. In
those, each variable occurs at exactly one place in the order, so no two
positions can ever share a code; sharing is available only along the tail
spine, never across the frontier. Currified naming buys sharing across
*positions*, and it is not an optimisation but a change in what is
representable in bounded space.

The price, stated honestly: a classifier relating coordinates in two
different positions **cannot be a single object of the theory** — both would
use the same names. Cross-position classifiers are therefore necessarily
structured by the shape, exactly as data is: a local query, then a residual
that travels (§7.3). This is the same statement as "no cross-leaf operation
is one object", and it is why §7 exists.

**Per-element finiteness, exemplified.** The carrier may be an infinite free
monoid and `ℛ` the recognisable subsets, coded by their minimal
recognisers: an infinite algebra, tier B, in which every *class* is finitely
named. The calculus never needs the algebra bounded, only the class coded.

---

## 3. Diagrams and the decomposition theorem

**Theorem 3.1 (canonical decomposition).** Let `X` be a nonzero
classification at sort `(V_h,V_t)`. There is exactly one expression

```
X = Σ_{i=1..n} P_i ⊗ S_i
```

with the `P_i ∈ ℛ(V_h)` nonempty pairwise disjoint **(D)** and the `S_i`
nonzero pairwise distinct **(F)**: the `S_i` are the distinct nonzero
**sections** `X(x,–)`, the `P_i` their fibres.

**Reading 3.2 (continuation congruence).** Define `x ~ x'` iff
`X(x,–) = X(x',–)` — prefixes indistinguishable under all continuations.
Theorem 3.1 says: the primes are its classes, constructed through the
partition window; the subs are the class *names*, compared through the
naming window. Dually `X` induces a congruence on the tail coordinate; the
node does not materialise it — the subs' own decompositions do, level by
level down the spine, bottoming out in leaf codes. **A representation is the
classification's congruence tower, materialised one cut per node and
deferred elsewhere.** The two directions named here recur as the two
dispositions of a crossing operation (§8), which is the same duality seen
from the operator side.

**Construction 3.3 (compilation).** From any finite rectangle expression,
insert incrementally into a maintained disjoint partition: for each
rectangle, meet against the cells already present, carve the overlaps out by
*relative* difference, join the subs where they coincide, and finally group
by sub. Enumerating sign-pattern cells gives the same canonical result and
is exponential; the incremental form is not.

The one cell whose expression would need a top — the all-negative pattern —
has section `0` and is deleted by the pointed discipline before it is
written: **non-unital heads and partial decompositions each fill exactly the
hole the other leaves.** Every prime ever written is a nonempty meet or a
nonempty relative difference of primes already present, so no cell is ever
expressed against the ambient carrier.

**The degeneracy ledger.** Canonical form outlaws the four ways of saying
one thing twice:

| rule | kills | licensed by | consumes |
|---|---|---|---|
| no zero subs | padding | `p ⊗ 0 = 0` (smash: definitional under pointing) | tail zero test |
| no zero primes | phantom classes | fibres of nonzero sections are nonempty | head emptiness (sound *and complete* — the one semantic decision canonicity leans on) |
| (D) | overlap | — | head disjointness |
| (F) | refinement | distributivity `(p∪p')⊗s = p⊗s ∪ p'⊗s` | tail equality |

**Definition 3.4 (diagrams).** `Diag(1)` = the terminal; `Diag(⟨A⟩)` =
canonical theory codes; `Diag(V_h,V_t)` = the adjoined `0`, or a finite
nonempty set `{(P_i,S_i)}` satisfying (D) and (F). Arcs draw from the
unpointed carrier; `0` enters only as an answer. Hashing makes equality a
pointer test and `0` an absence — the null reference is the one
legitimately sort-generic citizen precisely because it is not a citizen.

Zero-freeness maintains itself with no semantic test: smash makes `0` an
absence, absence makes emptiness at every composite sort a pointer test, and
free emptiness makes the invariant free to preserve. Tier E at every
composite sort is thereby synthesised from theory imports alone.

**Theorem 3.5 (internalisation; hierarchy as closure).** For any shape whose
head-position subtrees are hereditarily tier G, the diagrams over it form an
admissible support algebra (tier G; tier B if all leaves are B), coded by
their hashes. Hence `⟨Diag(V)⟩` is a legal import: **the calculus eats its
own output, and "hierarchical" is this closure property**, not a feature.
There is consequently **one diagram type, not two** — a prime over a
composite head *is* a diagram over that head, and internalisation is the
type hierarchy rather than a later construction. The typing law it induces:
head subtrees uniformly G, tail spines pay-as-you-go — *classes need
differences; names need a hash.*

**Reading 3.6 (the alphabet tower).** Along the frontier, a diagram is a
trim partial recogniser whose letters are *computed*: the letters at a node
are its primes, forced by Theorem 3.1; level k's alphabet is level (k+1)'s
state set. A leaf is the same pair *(recogniser, continuation)* with the
recogniser imported — a black box; an internal head is the recogniser
computed — a white box; internalisation says a white box can be re-boxed
black, which is why opacity is harmless. Strictness options are recogniser
hygiene: *partial = trim* (dead ends are absent transitions), *total =
completed* (totality costs a complement, tier B); *normalised = states
strictly sorted by level*, *trimmed = collapse of degenerate levels*. All
four corners are canonical where their prerequisites exist; none quotients
associativity.

**Definition 3.7 (representable).** `X` is representable over `V` iff at
every cut, hereditarily, its continuation congruence has finite index and
its classes are expressible in the head's algebra. Over finite shapes with
admissible theories this is automatic; over sequence-like theories it is a
per-element certificate. Representability is the *only* finiteness this
calculus demands.

---

## 4. Operations

**Definition 4.1.** Operations are **terms**, generated from

- **`filter(e)`**: the sub-identity at `e`. Filters only discard; they are
  exactly the behaviours `h ≤ id`.
- **`assign(π̄ ≔ f)`**: a complete morphism — total, single-valued —
  rewriting the coordinates at paths `π̄` by exported maps. Assigns only
  move; they never discard. The vector form (**parallel assign**) is a
  genuine primitive: sequential assigns cannot express simultaneous
  exchange without temporaries. Assigns are the sole position-addressed
  construct.

closed under composition `∘`, finite sum `+`, and star closure `∗`. A term
**acts on diagrams**, i.e. on codes: each generator's action is a theory
operation followed by re-canonicalisation. Hom-sets are pointed join
semilattices; composition is bilinear and `0`-strict; a term applied to data
may answer `0` — deadlock — and nothing is ever undefined.

Two consequences of taking terms-with-action as the definition:

- **Admissibility is a theorem, not a hypothesis.** The theories certify the
  generators, and closure under `∘`, `+`, `∗` preserves evaluability by
  construction. No operation is ever checked against a semantic
  admissibility condition.
- **No infinite sums are ever formed.** Pushforward on a code is one theory
  operation whether the class it names has three elements or continuum many.

**Definition 4.2 (the extensional shadow).** Where theories code singletons,
a term determines a pointwise kernel. **Extensional equality** is agreement
of kernels on all words. The shadow is the *specification* of equality — the
court in which identities are judged — and never the *mechanism* of
evaluation.

**The two faces.** A support element `e ∈ ℛ(V)` types two ways: as a
**point** ("these words") and as a **sub-identity** `filter(e)` ("keep these
words"). One canonical code, two typings; `e ↦ filter(e)` carries `∩` to `∘`
and `∪` to `+`. The two faces interconvert only where a top exists, and most
of the calculus never buys that bridge.

**Theorem 4.3 (the guard algebra).** The sub-identity interval `[0, id_V]`
is a **unital Boolean algebra even over non-unital supports**: its top is
`id`, which is free, and complement is `(¬h)(w) = {w}` iff `h(w) = 0` —
computed on data as a *relative* difference, tier G, no `⊤` anywhere.
Consequently `¬filter(e)` is an admissible *morphism* whose defining set
need not be admissible *data*: the morphism algebra strictly outruns the
data algebra.

**Definition 4.4 (`ite`).** Because a classification by a predicate yields
the **positive class alone** (§7.2), acting on both branches is a
construct rather than a pair of classifications:

```
ite(e, h_t, h_f)(d)  =  h_t(filter(e)(d))  +  h_f(d ∖ filter(e)(d))
```

extensionally `h_t ∘ filter(e) + h_f ∘ ¬filter(e)`, but computed with one
classification and one relative difference rather than two classifications
and a complement. Theorem 4.3 is what makes the second branch legal without
a top: the complement is taken *relative to the data actually there*.

**Proposition 4.5 (the operator semiring is half positive, the right half).**
The operator semiring is **zerosumfree** (`h + h' = 0 ⟹ h = h' = 0`: no
cancellation, hence no subtraction, hence monotone fixpoints) but **not**
zero-divisor-free (`filter(e) ∘ filter(e') = 0` with both nonzero when
`e ∧ e' = 0`: disjoint guards are honest zero divisors). Negation, banished
from the semiring by zerosumfreeness, survives exactly on the idempotent
interval, where the free unit gives it something to be relative to.

**4.6 On normal forms — a correction to draft 1.** Draft 1 canonicalised an
elementary term to a strictly alternating guarded word, merging adjacent
filters by meets. That is canonical and **anti-optimal**: merging two
single-coordinate guards produces a two-coordinate guard that must now
travel (§5), where separately neither would. The executable form keeps
guards *factored* by support.

More broadly, a canonical word is the wrong shape of answer. The right one
is a **confluent rewriting system** whose rules consult commutativity and
the *relative support* of operands to decide reordering, merging, and
refusal to merge. Independence of supports is the notion partial-order
reduction uses to justify not exploring both interleavings of independent
events; here it licenses exactly the same reorderings. And the reordering
such a system enacts is the reordering a schedule needs — so the normal-form
question and the scheduling question are one question seen twice. §14(ii).

**Structural maps.** Unitors; the computed associator; swap; theory
isomorphisms; marginalisation (tail joins, tier J); duplication. These are
the sort-changing citizens; they exist only along legitimate shape maps.

**The phenomenon ledger.** Each construct owns one phenomenon and provably
introduces no other:

| construct | owns |
|---|---|
| `filter` | partiality (deadlock) |
| `assign` | data movement |
| `∘` | sequencing, and guard conjunction |
| `+` | nondeterminism |
| `∗` | unboundedness |
| `case` | data-dependent control |
| the merge of §7.5 | quantification over a support |

Deadlock provenance traces to a filter; choice provenance to a `+`;
branching provenance to a `case`; any bounded universal to the merge. The
elementary layer performs no quantification, ever.

---

## 5. Skip, and blocks that travel

*This section is orthogonal to, and logically precedes, saturation. It is
about how a term reaches the level where it acts.*

**Definition 5.1 (footprint and top).** The **footprint** of a term is the
set of positions it reads or writes. Its **top** is the highest level its
footprint reaches. A term **skips** a level whose subtree its footprint does
not meet.

**Proposition 5.2 (skip is compositional, uniformly).** A term skips a level
exactly when every one of its parts does, and this rule is the same for
`∘`, `+` and `∗`.

**Corollary 5.3 (blocks travel).** A term descends **as one object** for as
long as it skips. Nothing about it is decomposed, examined or rebuilt on the
way down; the levels above its top transmit it unchanged, which is
discipline 3 as an operational statement.

**5.4 At the level a block cannot skip**, it *parenthesises*, splitting along
the two congruence directions of Reading 3.2: the part acting on the head
goes to the **edge**, applied to the primes; the part acting on the tail
goes **down**, applied to the subs; the node rebuilds through Construction
3.3.

```
apply(H, V, d):
    if H skips V:            return d
    if V is a leaf:          return theory.apply_local(d, H)
    split H across the cut → (edge part, tail part, crossing part)
    for (P, S) in d:  emit ( apply(edge part, V_h, P), apply(tail part, V_t, S) )
    canonicalise
```

**Proposition 5.5 (only `∘` factors).** A composition of parts with disjoint
supports is a tensor and factors into edge and tail. **A sum does not**:
`(h_e ∘ h_t) + (g_e ∘ g_t) ≠ (h_e + g_e) ∘ (h_t + g_t)`. A sum therefore
travels intact only while its summands skip *together*, and splits into
separate applications, joined at the level where they part company.

**5.6 The theory receives a term, not an opcode.** At a leaf, the whole
maximal local term is handed over (Definition 2.3). This is where the
per-theory normal form lives, and it is the reason `split_equiv` is *not*
the mechanism for operations: a theory with a native operation must never be
made to split a code, act per piece and rejoin.

**5.7 The footprint notion, honestly.** A footprint computed statically from
a term's syntax **over-approximates**. An operation reading `tab[x]` has a
static footprint covering all of `tab`, while its actual footprint depends
on the data. The minimal notion is dynamic and is what one actually wants.
Static footprints are adequate exactly while every operation's reach is
static — which is a real fragment, and everything in §6 is stated over it.
§14(iii).

---

## 6. Saturation

Saturation is not a schedule bolted onto a finished semantics. It is what §5
already does when the travelling block happens to be a star, made explicit.
Its whole content is that **the split is derived from footprints; nothing is
declared.**

**Definition 6.1 (the decomposition at a cut).** Write the term being
saturated as

```
H = (L + G + F + id)*
```

split by where each summand's footprint lies relative to this cut:

| part | footprint | disposition |
|---|---|---|
| `F` | tail only | skips this level; belongs downstream |
| `L` | head only | local to this level's edge |
| `G` | crosses the cut | anchored here, by Definition 5.1 |

`L`, `G`, `F` are each sums of compositions, and `id` sits in the sum so
each closure is reflexive.

**6.2 Throwing a crossing event in two directions.** A `g` in `G` that is
**Kronecker** factors as `g = l ∘ f`, a head part and a tail part. It then
does *not* have to act at this level — which is the surprising point, since
one expects "crossing" to mean "anchored, do the work here". Instead it is
thrown both ways, with re-saturation fused into its own application so that
nothing is left unsaturated behind it:

```
down the tail:   (F + id)* ∘ f
on the edge:     (L + id)* ∘ l
```

**Selections first, on both sides.** `g` fires only if both members find
friends. Launching `(F + id)*` — an unbounded fixpoint — before testing that
`l` has a match is speculative at best and disrupts the semantics at worst,
since the other member may return the empty set. Apply the guards of both
members, drop the pair if either is empty, and only then apply the updates
and re-saturate.

**6.3 The schedule at a node.**

```
saturate(H, V, d):
    if V is a leaf:   return theory.apply_local(d, H)
    d := (F + id)*  on the subs                     # downstream first
    d := (L + id)*  on the primes                   # then local
    repeat until d is stable:
        for g in G:                                 # chaining
            d := (g + id)(d)                        # thrown both ways, as 6.2
```

`(F + id)*` runs before the first `g` is ever touched, and the local
saturation runs too. The `G` loop then **chains** — each `g` applied to the
accumulated result in turn, rather than the whole sum iterated jointly.

`g(d)` at a node with pairs `(P,S)` is the tensor: `((L+id)* ∘ l)` on `P`
and `((F+id)* ∘ f)` on `S`, rebuilt by Construction 3.3.

**6.4 The hierarchy is automatic.** The two child saturations are this same
procedure one level in, bottoming out where a theory closes its own local
term. No annotation, no user-supplied locality, no declared decomposition:
the split falls out of the footprints, so the procedure applies at every
sort of every shape.

**6.5 Why this is not round-based iteration.** Iterating `X ↦ d + h(X)` by
rounds rebuilds a fresh object each round, so every round misses the caches
and the cost is rounds × events × |X| in freshly built nodes. In 6.3 the
fixpoints sit *inside* the operator terms, so memoisation keys on
**saturated nodes**: a node is saturated once, ever. That is the whole
difference, and it is a difference in what is cacheable rather than in how
many operations are performed.

---

## 7. `split_equiv`: the backbone

The calculus decomposes against classifiers other than its own continuation.
Everything that inspects data is a caller of one traversal.

**Definition 7.1.** For a classifier `E` and data `d`,

```
split_equiv(V, d, E)  →  a partition of d, labelled
```

partitions `d` into the pieces on which `E` has each realised value or
residual. Zero pieces are absent, so the label set *is* the discovered
alphabet: empty classes are never represented, and pieces that agree are
merged before any client sees them.

**Definition 7.2 (labels, and the boolean case).** A label is the value of
the local expression that was split on. The requirements on labels are
exactly the requirements on codes — **canonical, stable, hashable** — and
nothing more; the calculus never interprets them. Any finite domain will
serve, and a theory chooses the one natural to it: the value itself for an
arithmetic field, a co-factor for a relational theory, an index into
whatever the theory privately keeps. Transcoding to integers and back is
needless.

**Booleans are not a label domain.** A classification by a predicate yields
**one** class — the positive one. There is no negative class, because
naming it would require a complement, and because a classifier's business is
to say what *is* the case. Where both branches are wanted, Definition 4.4's
`ite` provides them with one classification and one relative difference.
Consequently `filter` is not a special construct that happens to resemble a
classification: **`filter` is a classification, read for its positive
class.**

**7.3 The traversal, in three movements.**

**Down.** Travel to the first position the classifier mentions. A classifier
mentioning nothing in a subtree returns there in one class without
descending: locality is the distance-zero case and it is free.

**Across.** Meeting a coordinate substitutes its class and renormalises, so
the travelling classifier stays ground and mentions only coordinates not yet
consumed; a consumed coordinate can never be re-queried. Classifiers being
interned, grouping head-classes by residual code and keying the memo table
on that code are the same act — **deduplication is cache sharing**.

Normalisation must apply *while a residual curries*, not only when a client
writes a classifier: currying is when residuals get their chance to
coincide. An applied operation therefore carries the builder that made it.

**Up.** Results federate; §7.5.

**Definition 7.4 (kernel and labelling).** A partition is stored as its
**kernel** — the canonical, interned tuple of pieces — plus a **labelling**
from labels into it. Splitting the two is what lets partitions federate:
agreeing on the kernel is a pointer test, and disagreeing on the labelling
costs a finite table.

**7.5 The merge.** Two subqueries whose residual codes differ but whose
realised partitions agree are recognised on return and share one kernel,
keeping only their relabellings apart. Every partition federates, including
those that did not descend and those with a single class: not descending is
a locality optimisation, but not federating would let equal partitions carry
different kernels and the merge would stop being an equivalence.

This is where weak normalisation is repaid retroactively, and where the
**equivariant collapse** appears — not as a code path, but as what the merge
finds when the structure is there.

**Theorem 7.6 (canonicity of the quotient).** By the pieces alone: empty
classes are never represented; classes with equal parts are merged before
any client sees them; hence the discovered alphabet is the **coarsest
refinement compatible with `E` on `d`** — minimal, canonical, forced.
Per-query alphabet discovery inherits the uniqueness of Theorem 3.1
wholesale.

**7.7 Callers.** All of them are this traversal with a different codomain
and a different action on the pieces:

| caller | codomain | action |
|---|---|---|
| `filter` | a predicate | keep the positive class; over one coordinate, does not travel |
| `assign(x ≔ e)` | a value sort | write the class's label on each piece |
| `Θ` | any sort | keep all classes, labelled |
| `case` | any sort | apply the branch for that label, **once per class** |
| the default classification | the unit sort | one class: acceptance |

The tautological instance — `E` the continuation itself, its codomain
existing by internalisation (Theorem 3.5) — reproduces the structural
decomposition: **the calculus classifies against its own values, and its
normal form is the special case of its own quotient operator.**

**7.8 Cost, structurally.** Per cut crossed between the classifier's support
and the queried cut, the traffic is

```
(# distinct realised normalised residuals — after merge: distinct kernels, plus finite tables)
  × distance (levels crossed)
  × class-code size in the theory
```

The first factor is a property of the classifier on the realised object; the
second is the shape's business, and locality is the distance-zero case; the
third is the theory's business. Which classes a theory can *name cheaply*
and which cuts communicate cheaply are the two axes of modelling in this
calculus. No a-priori bound can match a per-query bill, since a-priori
bounds cannot see the data.

---
**Ex2 (residue) — the curried walk.** Shape `(⟨b⟩,(J₁,(J₂,⟨c⟩)))` with `b, c`
ranging over a finite interval and the junk sorts unconstrained. Classifier
`(b+c) mod 3`.

At `⟨b⟩` the residuals normalise to exactly three codes — three subqueries
**regardless of the range of `b`**. They travel the junk curried and ground;
the junk contributes nothing and re-queries nothing. On return the merge
finds one shared kernel — the cosets mod 3 — with three relabellings: one
kernel memoised, finite tables kept.

The classes at `⟨b⟩` are `{0,3,6,…}`, `{1,4,7,…}`, `{2,5,8,…}`: **not
intervals, and not elements** — Theorem 3.1 forces these classes to exist,
and whether they have cheap codes is the theory's commitment. Over
finite-unions-of-intervals, the first costs a number of intervals linear in
the range; over an ultimately-periodic code family, it is one constant-size
code at any range. Same tower, same three labels, different bill — the third
cost factor, isolated.

---

## 8. The general crossing case

When `g` in `G` is **not** Kronecker, its two ends are *correlated*: which
operation the tail applies depends on what the edge saw, or the reverse. The
level can no longer relink blindly, so it quotients. This is `case` at a
cut, with the branch table not declared but **discovered** — the branches are
`g`'s own curried residuals.

Three dispositions.

**8.1 Split the edge, specialise the tail.** Grab the value the operation
needs from a coordinate in the head, `split_equiv` the edge on it, and for
each class apply the correspondingly curried operation to the tail; then
relink each edge class to the tail that received *its* class's operation.

```
for (P, S) in d:
    for (label, P_class) in split_equiv(V_h, P, E):
        S_class := apply( g curried by label, V_t, S )
        emit (P_class, S_class)
canonicalise
```

The edge classes are disjoint, so **(D)** survives for free; two classes
whose specialised operations happen to agree produce equal tails and **(F)**
merges them. Canonicalisation does the rest — nothing here maintains the
invariants by hand.

**8.2 Split the tail, specialise the edge.** The mirror image: quotient the
tail to determine the classes, apply a different treatment to the edge per
class, relink. Here the resulting edge parts may *overlap* rather than
partition, which Construction 3.3 also absorbs, unioning the subs of pieces
that coincide.

**8.3 Split both, congruently.** When both ends read as well as write — a
guard relating a head coordinate to a tail coordinate is the archetype —
both sides are quotiented and the classes matched by label, only the
agreeing pairs surviving.

**8.4 The choice is a cost decision, not a canonical one.** The two
directions are Reading 3.2's two congruence directions seen from the
operator side: the node materialises one of them and defers the other, and
which it materialises is exactly the choice above, priced by the three
factors of §7.8.

**8.5 Label-based synchronisation, and why this is the expressiveness
claim.** Read 8.1–8.3 as a *synchronisation discipline*: the parts of a
shape are arbitrary, mutually ignorant theories, and a crossing operation is
a rendezvous between them conducted entirely in labels. One side offers a
label; the other applies the operation indexed by it; the level relinks.
Neither side needs to know the other's algebra — only that labels compare.

That is what lets sorts be **mixed freely**: a Boolean block, a bounded
integer field, a word algebra, and a nested diagram may all sit in one shape
and still interact, because interaction is mediated by discovered labels
rather than by a common representation. Declared synchronisation alphabets
are the familiar version of this; what is added here is that **the alphabet
is discovered per query** — minimal, canonical, and forced by Theorem 7.6
rather than chosen by a modeller. Systems whose every event is a tensor of
local actions never invoke it at all (§11); systems that need genuine
correlation pay for exactly the correlation they use.

---

## 9. Theories, worked

**9.1 Enumerated.** An explicit finite carrier, coded by subsets. Tier B
trivially; `split_equiv` by enumeration. Its purpose is to isolate
calculus-level questions from theory-level ones: everything it does is
obviously correct, so a discrepancy is the calculus's. It is also the
baseline every other theory should beat, since enumeration is the one thing
a theory can do badly.

**9.2 Boolean blocks (BDD).** A block of Boolean bits, coded by binary
decision diagrams over a **currified** variable set (§2.6): one set of
variables ever, indexed from the terminal upward, shared by every position.
Codes are canonical and hashable by construction, and all four tiers come
free.

- **Operations are relational products.** The theory receives a maximal
  local term (Definition 2.3) and fuses *all* of it: composition becomes
  relational composition, `+` becomes disjunction of relations, `∗` becomes
  transitive closure — one relation over interleaved current/next variables,
  applied once. Interleaving is the only layout for which the product is
  efficient. Splitting a code, acting per piece and rejoining would be a
  traversal where the theory has a single native operation, and is exactly
  what Definition 2.3 forbids.
- **`split_equiv` has several query forms.** By a *predicate*: the positive
  class, one meet (Definition 7.2 — there is no negative class). By a *cube*
  naming which variables to split on: the realised valuations of that cube,
  labelled by those valuations — this is the "grab the value of a coordinate"
  of §8.1. By a *formula over the block and beyond*: the distinct co-factors,
  which is Theorem 3.1 one level down, performed by the theory.
- **Consequence of currification.** A classifier relating two positions is
  never one diagram of this theory, since both positions use the same
  variables. It is a local query plus a residual that travels — §2.6, and
  the reason §7 is structured as it is.

**9.3 Bounded integer fields (FDD).** Finite domains encoded over the same
currified Boolean variables: a field is a group of bits, `field = value` is a
code, and everything in 9.2 carries over unchanged. `split_equiv` by a field
is the cube form, and **its labels are integers whose meaning is the value**
— the one place where a numeric label is not merely convenient but
canonical. A bounded place/transition system is then one field per place,
transitions as relational products, every event Kronecker, and §6 applies
directly.

**9.4 Recognisable sets.** The paradigm infinite theory: minimal recognisers
as canonical codes, tier B, concatenation and quotient as exported maps.
Every touched class is finitely coded though the algebra is infinite —
discipline 4, exemplified.

---

## 10. Extension versus intension

**Theorem 10.1 (local constancy).** The graph of a total map is a finite
union of rectangles iff the map has finite image. **Corollary:** the diagonal
of an infinite carrier has no finite rectangle form — the extensional
encoding cannot express *no-op*, while the calculus has `id` free twice
(empty composition; `assign(π ≔ π)`). Over finite carriers every relation of
out-degree `d` is a sum of `d` guarded assigns, so the calculus subsumes
extensional encodings, strictly at infinite theories. Filters, by contrast,
*are* data: the asymmetry of the elementary basis — selectors reduce to
data, complete morphisms do not — is this theorem speaking.

**Definition 10.2 (trace).** The trace of `h` on explored `X` is
`graph(h) ∩ (X × ⟦V⟧)`. On any finite fragment the restriction has finite
image, so the trace is rectangle-representable *precisely and only per-run*:
memoisation is the runtime construction of the finite-index certificate. The
extensional encoding is what one gets by paying for that certificate before
the run.

**The unit table.** One principle, four appearances — *never demand that a
unit be a citizen; let it be the empty operation of the relevant monoid*:

| monoid | unit | status |
|---|---|---|
| `(+,0)` on operations | `0` | adjoined — answer, never letter |
| pairing on data | `⊤` | never demanded — non-unital heads, partial decompositions, the sink unmaterialised |
| `(∘,id)` on operations | `id` | free — the empty composition, and the content of discipline 3 |
| rectangles across a cut | diagonal | unavailable over infinite carriers — the missing unit of the block ideal |

The extensional world is the world that insists on representing its units as
objects, and it is exactly over finite or tier-B structure — where units
happen to be representable — that the two worlds coincide.

---

## 11. Cuts, ranks, and the two regimes

Across a fixed cut, data has **section rank** — the number of primes, forced
by Theorem 3.1. Operations have **tensor rank** — the least `n` with
`h = Σ_{m≤n} ⊗_ℓ h_{m,ℓ}`. Tensors of elementary terms are elementary, so a
block-presented system stays in normal form; block operations along a cut
form a **non-unital ideal**, missing exactly the diagonal (Theorem 10.1).

Two regimes for the interaction alphabet across a cut:

- **Declared:** a finite letter set fixed a priori — an upper bound on tensor
  rank, constraining only the interface. Choice is externalised to the
  letters; within a letter, deterministic.
- **Discovered:** the per-query minimal alphabet of §7 — canonical,
  coarsest, priced by transport.

**The declared regime is exactly the fragment in which `split_equiv` is
never invoked, and it is exactly the Kronecker-consistent fragment**: every
event a tensor of local actions, every crossing operation factoring as
`l ∘ f`, §6.2 applying and §8 never being reached. Place/transition systems
live there permanently, under *any* decomposition. That is why they saturate
so well, and why a classifier travelling in such a system would be a symptom
of bad encoding rather than of expressive power.

The discovered regime removes the bound at the stated cost. The parallel is
precise: *declared letter sets are to discovered alphabets as declared leaf
partitions are to discovered primes* — upper bounds a modeller accepts in
exchange for never transporting.

One invariant governs both ranks and the §7.8 bill: **residual divergence** —
how fast the residuals of realised classifiers separate as the head varies.
On Ex1 every protocol-relevant classifier's residual depends on the head only
through the two boundary forks — at most four residuals, ever, at any size.
On Ex2 residuals separate only up to residue — three, at any range. When
residuals separate as fast as the carrier — two coordinates constrained
equal across a cut is the canonical case — no mechanism helps, and §13 says
the one thing this document says about it.

---

## 12. The streaming factorisation

Call a presentation a **finitary stream** if it is given by finite control
and finitely many registers valued in declared sorts, consuming the frontier
left to right, each step emitting elementary actions parameterised by the
letter read and the register contents, with admissible register updates.
Two hypotheses, both load-bearing: the stream's transition *tests* are
admissible predicates, and the configuration is *declared* — control finite,
every register sort an existing sort.

**Theorem 12.1 (factorisation; proof owed, §14(v)).** Every shape-respecting
finitary stream satisfying both is extensionally equal to a term of the
calculus over theory-internal maps, filters, parallel assigns,
tensor-with-constants, structural maps, `case`, `+`, `∘` — with, over finite
shapes, **no `∗` at shape level**: iteration survives only inside
sequence-structured theories. *Star-free over the shape; stars only in the
alphabets.*

**Construction.** Thread the configuration as data; branch only on control.
Enlarge the sort by a configuration component, initialise by
tensor-with-constant, and let the stream become a straight-line composition,
one term per frontier leaf, associated to the shape tree by structural maps.
Each per-letter step splits along the phenomenon ledger: the control
successor has finite image, hence is blockwise by Theorem 10.1 — a `case`
whose discovered alphabet is bounded by it; the register and emission
updates are possibly infinite-image but single-valued — one parallel assign
per branch. Finally marginalise the configuration away. Finite shape means
finitely many steps, hence no `∗` at shape level.

**Conjecture 12.2 (adjoined configuration).** A stream with genuine cross-cut
register flow is inexpressible over the original shape alone: the detour
through the enlarged sort is unavoidable, and the *least* adjoined
configuration sufficient for a behaviour is an invariant of it —
conjecturally the tensor rank of §11 in another gauge.

---

## 13. Remark on compression, at arm's length

The size of a representation is the sum over nodes of their prime counts,
and a node's prime count is fixed by Theorem 3.1: the number of distinct
realised sections at its cut. It is small exactly when the parts the shape
separates are mostly independent — when residual divergence is slow — and
Ex1 shows that natural systems with local communication achieve it. Choosing
the shape that minimises total size contains variable ordering for flat
decision diagrams, so it is NP-hard, and some classifications are large
under *every* shape — two coordinates constrained equal over a large carrier
force as many primes as carrier values at any cut separating them.

Currification adds a second axis the flat setting does not have: sharing
across *positions*, not merely along the spine. A shape with many isomorphic
subtrees pays for its local structure once. This is why the philosophers'
representation is linear in the number of philosophers rather than merely
sub-exponential, and it is not available to any encoding in which each
coordinate occupies its own place in a global order.

This document defines the calculus and prices its operations. Whether a
given semantics admits a shape and theory encoding under which it compresses
is the modeller's problem, and this document deliberately says nothing more
about it.

---

## 14. Obligations and seeds

In order of load-bearing-ness.

**(i) The transport lemma.** One lemma, two faces. *Mechanism face:* the
curried residual traversal of §7.3 computes the quotient — soundness of
currying, of code-level deduplication, and of the retroactive merge, with
the three-factor bill of §7.8 as the proved cost. *Staging face:* per-node
continuation congruences, composed along the spine, reconstruct the full
congruence of the classified object — the formal content of "a
representation is the congruence tower" (Reading 3.2). The bet is that these
are one induction read in two directions.

**(ii) The rewriting system.** §4.6 states the shape of the answer without
giving it: confluent rules over commutativity and relative support,
canonical representatives for sums of terms modulo the semiring laws, the
merge laws, and independence. Prove confluence, and prove that the schedule
of §6.3 is expressible as its rules — the claim that the normal-form
question and the scheduling question are one question. Expected hard,
expected central.

**(iii) The dynamic footprint.** §5.7. A minimal, data-dependent notion of
what an operation reaches, with §5 and §6 restated over it. Everything in
this draft is stated over static footprints, which is a real fragment but
not the intended one.

**(iv) The direction rule.** §8.4 says the choice between splitting the edge
and splitting the tail is a cost decision and leaves it there. A rule — even
a heuristic with a stated regret bound — would close the last place where
the calculus defers to a modeller inside an operation rather than at the
interface.

**(v) The factorisation proof.** Complete Theorem 12.1 by the threading
construction, with cost transparency; then attack Conjecture 12.2.

**(vi) The generated category, stated once.** §4 defines operations as terms
with a code action and demotes the kernel to the extensional shadow. State
this with nothing informal left: which terms, which actions, which quotient,
and the closure argument that evaluability is preserved by `∘`, `+`, `∗`.
Then `hom(1,(V_h,V_t))` as a tensor of pointed modules with Theorem 3.1 as
its canonical-basis theorem, making the qualitative-primes gauge functorial
rather than legislated.
