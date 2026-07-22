# Hierarchical Shape Calculus

*Working draft 3. Standalone: every object is defined from nothing, and the
document owes nothing. It is written to be the seed of its own iteration — a
reader holding only this text should be able to continue the work. §15 lists
where to push.*

*What moved since draft 2. The operation layer collapsed. Draft 2 had a basis
of `filter` and `assign` closed under the semiring, `split_equiv` as a
traversal serving them, saturation as a schedule, and a separate "general
crossing case". Draft 3 has **two constructs** — a local term handed whole to
a theory, and a **query/case bracket** — from which filters, assigns, `ite`,
saturation's crossing events and the crossing dispositions are all obtained.
The case is the **leader**: it owns its criteria, decides what to split, may
adapt, and returns a morphism; its only law is linearity, and everything else
about it is an assume/guarantee contract on its author. `skip` is no longer
defined from footprints but from `id`, which promotes `id` from afterthought
to primary. Labels are residuals, and they are **scoped to a case**. Crossing
criteria are typed in a declared **interchange theory**. Coefficient
semirings, multi-terminal values and inverse images remain out of scope.*

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
the calculus itself (§7–8).

The ambition, stated once so the rest can be read against it: **hierarchy
over arbitrary sorts mixed freely, with one mechanism general enough to
mediate operations between the algebras of the parts.** That mechanism is
label-based synchronisation, the labels are *discovered* rather than
declared, and the construct that opens and closes the discovery is the
**case** (§8). Everything below is in service of that sentence.

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

3. **Skip is `id`-propagation.** An operation whose restriction to a subtree
   is `id` transmits nothing there, because `id` is free and free things are
   not represented. That is the whole of skipping (§5). Discipline 2 is
   where a node's ignorance is legitimate; discipline 3 is where an
   operation's is; and both are consequences of what is *not* a citizen.

4. **Finiteness per element.** No carrier, alphabet or algebra is required
   finite. What is required is that each *represented class* admit finite
   canonical codes and that each *run* of an operation touch finitely many
   codes. Global guarantees are replaced by per-element and per-run
   certificates, discovered rather than declared.

**Lineage, stated plainly.** The structural normal form generalises the
compressed, trimmed decision-diagram forms over variable trees; "primes" and
"subs" are used deliberately in that tradition's sense, with the shape tree
in the role of the vtree. The continuation congruence is the classical right
congruence of word classification. The local layer is guarded-command
territory. The cross-cut layer is theory-combination territory: a mixed
criterion purified by the shape into per-theory parts that exchange only
values in a shared sort.

The claimed delta, three items:

- **Currified naming.** A code is relative to its position and therefore
  shared across isomorphic positions — which no fixed global variable order
  can express, since there each variable occurs at exactly one place (§2.6,
  §10, §14).
- **Skip as `id`-propagation**, from which operations that travel as opaque
  blocks, and hierarchical saturation, are *derived* rather than retrofitted
  (§5, §6).
- **The query/case bracket.** A scoped, author-certified, linear
  presentation of morphisms at a cut, whose vocabulary is discovered
  residuals rather than a declared synchronisation alphabet, and which is
  the single mechanism by which mutually ignorant theories interact (§7,
  §8).

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
  subterms are one object under hashing. Naming, where needed, is a surface
  decoration erased at declaration time (§9). §2.6 pushes this into the
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
the normal form (§8.7): the normal form is the special case of the case
construct whose criterion is the tautological one. Nothing richer lives at
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
composition, sum, star closure — whose support lies in this leaf, handed
over for the theory to interpret however it likes. A relational theory fuses
the term into a single relation and applies it once; an enumerated theory
executes the parts. **The normal form for a local term is per theory; the
calculus delivers the term and does not prescribe the fusion.** Splitting a
code, acting per piece and re-joining is what a theory with a native
operation must never be made to do.

**Definition 2.4 (normalisation strength).** A theory exports canonical
codes not only for its support elements but for the closed terms of its
operation and criterion languages — the expressions that travel as residuals
(§7). The strength of this normalisation is a declared property, and a
**cost parameter, never a soundness parameter**: undetected equalities cause
redundant subqueries, which the merge of §7.6 cleans up retroactively; they
never cause wrong answers.

**Definition 2.5 (`split_equiv`).** The theory's partition primitive:
partition a code by the residual of a criterion, returning the realised
classes indexed by residual. Contract and typing in §7.1. Enumerating the
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
four of them. This is "no names" pushed inside the theory: positions are
paths, and two equal subterms are one object. Internalisation (Theorem 3.5)
is the same phenomenon for the diagram theory, where hash-consing supplies
position-relativity for free; §14's sharing claim covers leaves and internal
nodes by one argument.

It is also where this calculus departs from vtree-based decision forms. In
those, each variable occurs at exactly one place in the order, so no two
positions can ever share a code; sharing is available only along the tail
spine, never across the frontier. Currified naming buys sharing across
*positions*, and it is not an optimisation but a change in what is
representable in bounded space.

The price, stated honestly: a criterion relating coordinates in two
different positions **cannot be a single object of the theory** — both would
use the same names. Cross-position criteria are therefore necessarily
structured by the shape, exactly as data is: a local query, then a residual
that travels (§7.3). This is the same statement as "no cross-leaf operation
is one object", and it is why §7 and §8 exist.

**Definition 2.7 (interchange theory).** A criterion whose subterms are
evaluated in different theories needs a sort in which those theories agree.
That sort is declared, and the theory over it is the **interchange theory**.
Two facts fix its role:

- **Equality is always available.** The degenerate interchange theory —
  labels compared only for equality — exists for any pair of theories and
  supports pure rendezvous. This is why arbitrary sorts can synchronise at
  all.
- **Linear integer arithmetic is the working one.** Index expressions,
  arithmetic guards across a cut, and array addressing all live there. It is
  what makes `tab[i]` and `a < b + c` mean anything when `a`, `b`, `c` sit
  in different leaves.

The calculus never interprets a residual. But the *theories at the two ends
of a criterion* must interpret it identically, and the interchange theory is
where that agreement is declared. Types are declared; alphabets are
discovered.

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
dispositions available to a case (§8.5), which is the same duality seen from
the operator side.

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

**Corollary 3.6 (internalisation carries operations).** A theory must export
`split_equiv` (Definition 2.5). Diagrams are a theory. Therefore **diagrams
must export `split_equiv`**, and §8's construction is that export: what a
case does at a cut is the diagram theory answering a partition query about
itself. The consequence for the architecture is that there is no "leaf case"
and "node case" — there is one construct, whose leaf instance is delegated
and whose node instance is §8.

**Reading 3.7 (the alphabet tower).** Along the frontier, a diagram is a
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

**Definition 3.8 (representable).** `X` is representable over `V` iff at
every cut, hereditarily, its continuation congruence has finite index and
its classes are expressible in the head's algebra. Over finite shapes with
admissible theories this is automatic; over sequence-like theories it is a
per-element certificate. Representability is the *only* finiteness this
calculus demands of *data*; §8.6 states the one it demands of *operations*.

---

## 4. Operations: the basis

**Definition 4.1 (the basis).** Operations are terms:

```
H ::= id                       -- free; never represented
    | local(t)                 -- a maximal local term, handed whole to a theory
    | query(E) ; case          -- the crossing bracket (§7, §8)
    | H ∘ H  |  H + H  |  H*
```

A term **acts on diagrams**, i.e. on codes, and each action is followed by
re-canonicalisation. Hom-sets are pointed join semilattices; composition is
bilinear and `0`-strict; a term applied to data may answer `0` — deadlock —
and nothing is ever undefined.

**4.2 Where filter and assign went.** They are not generators. They are

- **primitive inside `local`**: a guard, an assign, a parallel assign whose
  support lies in one leaf never becomes an opcode at all — it is a subterm
  of the maximal local term the theory fuses (Definition 2.3);
- **derived across a cut**: a guard or an assign relating coordinates in
  different subtrees *expands* into `query ; case`, and that expansion is
  its definition (§8.4).

This asymmetry is the honest content of "filter and assign are conveniences".
They are convenient names for the two things one usually wants; locally they
are the theory's business and the calculus need not know their shape;
globally they are cases like any other. Draft 2 presented them as uniformly
primitive, which is exactly why crossing guards and crossing assigns had no
definition there.

**Definition 4.3 (the extensional shadow).** Where theories code singletons,
a term determines a pointwise kernel. **Extensional equality** is agreement
of kernels on all words. The shadow is the *specification* of equality — the
court in which identities are judged — and never the *mechanism* of
evaluation.

**The two faces.** A support element `e ∈ ℛ(V)` types two ways: as a
**point** ("these words") and as a **sub-identity** ("keep these words").
One canonical code, two typings; the second carries `∩` to `∘` and `∪` to
`+`. The two faces interconvert only where a top exists, and most of the
calculus never buys that bridge.

**Theorem 4.4 (the guard algebra).** The sub-identity interval `[0, id_V]`
is a **unital Boolean algebra even over non-unital supports**: its top is
`id`, which is free, and complement is `(¬h)(w) = {w}` iff `h(w) = 0` —
computed on data as a *relative* difference, tier G, no `⊤` anywhere.
Consequently the negation of a guard is an admissible *morphism* whose
defining set need not be admissible *data*: the morphism algebra strictly
outruns the data algebra. A derived predicate such as *deadlocked* — the
complement of the union of all enabling guards — is therefore computable
against the data present, with no top anywhere in sight.

**4.5 `ite` is not a construct.** Draft 2 defined it as one classification
plus a relative difference, to work around the rule that a predicate yields
only its positive class. That rule is gone (§7.2): a criterion is free to
name its negative class rather than junk it, and the negative class is
codeable at tier G by relative difference against the data present. So

- a **filter** is a case whose criterion junks the negative class;
- an **`ite`** is a case whose criterion names it.

Same query, same machinery, different criterion. Theorem 4.4 is still what
makes the second legal without a top; it now lives inside the query rather
than beside it.

**Proposition 4.6 (the operator semiring is half positive, the right half).**
The operator semiring is **zerosumfree** (`h + h' = 0 ⟹ h = h' = 0`: no
cancellation, hence no subtraction, hence monotone fixpoints) but **not**
zero-divisor-free (disjoint guards compose to `0` with both nonzero: honest
zero divisors). Negation, banished from the semiring by zerosumfreeness,
survives exactly on the idempotent interval, where the free unit gives it
something to be relative to.

**4.7 Deadlock is exact.** A class pair contributing nothing contributes the
**empty sum**, and the empty sum is a value of the same construction that
produces every other. `0` is not a failure mode of the nonzero answers; it
is the additive unit. A guard that does not hold produces `0`, and that
answer is exact — nothing was lost, because nothing was there. Consequently
no run is ever an under-approximation on account of a junked class, and no
report is owed. (Where a modeller wants to know that a guard never fired,
that is a counter in the invoice of §9.5, i.e. profiling, not algebra.)

**4.8 On normal forms.** A canonical word is the wrong shape of answer. The
right one is a **confluent rewriting system** whose rules consult
commutativity and the *relative support* of operands to decide reordering,
merging, and refusal to merge. Merging two single-coordinate guards into a
two-coordinate guard is anti-optimal: the merged guard must now cross a cut,
where separately neither would. Independence of supports is the notion
partial-order reduction uses to justify not exploring both interleavings of
independent events; here it licenses exactly the same reorderings. And the
reordering such a system enacts is the reordering a schedule needs — so the
normal-form question and the scheduling question are one question seen
twice. §15(ii).

**Structural maps.** Unitors; the computed associator; swap; theory
isomorphisms; marginalisation (tail joins, tier J); duplication. These are
the sort-changing citizens; they exist only along legitimate shape maps.

**The phenomenon ledger.** Each construct owns one phenomenon and provably
introduces no other:

| construct | owns |
|---|---|
| `id` | nothing — it is the free unit, and the content of skip |
| `local` | whatever the theory owns: local partiality, local movement, local choice |
| `query` | class discovery — the alphabet, and quantification over a support |
| `case` | interpretation of classes, and re-tensoring after decorrelation |
| `∘` | sequencing |
| `+` | nondeterminism across levels |
| `∗` | unboundedness from iteration |
| a case's query program | unboundedness from indirection — contract, §8.6 |

Deadlock provenance traces to a criterion; choice provenance to a `+` or to
a case reply of rank `> 1`; branching provenance to a case; any bounded
universal to the merge of §7.6.

---

## 5. Skip, and blocks that travel

*This section is orthogonal to, and logically precedes, saturation. It is
about how a term reaches the level where it acts.*

**Definition 5.1 (skip).** A term `H` **skips** a subtree `V` iff its
restriction to `V` is `id`:

```
skip(H, V)   ⟺   H|_V = id
```

Nothing more. `id` is free, so a skipped subtree receives nothing, and the
level above it transmits `H` unchanged. Discipline 3 is this definition.

**5.2 How the skip question is answered.** `skip` is a *query posed to the
term*, and several oracles may answer it, in increasing precision:

- **Static support.** The set of positions the term's syntax mentions. If it
  misses `V`, the term skips `V`. Cheap, sound, and adequate exactly while
  every operation's reach is static.
- **Resolved indirection.** After a query grounds an index, the residual's
  reach is exact: `tab[i]` mentions all of `tab` statically, but per
  discovered class `i = ℓ` the residual mentions the single position
  `tab[ℓ]`. The dynamic reach is the union of the residuals' static reaches
  over *realised* classes, so one pays for the cells the data actually
  visits.
- **Theory declaration.** A theory that knows a local term is the identity
  on part of its own state may say so.

Draft 2 defined skip *from* the static footprint, and therefore had to
concede that skip over-approximates and to owe a "dynamic footprint"
obligation. Defining skip from `id` and treating supports as one oracle
among several removes both the concession and the obligation: the
indirection case is handled by the query bracket that was going to be needed
anyway.

**Proposition 5.3 (skip is compositional, uniformly).** A term skips a
subtree exactly when every one of its parts does, and this rule is the same
for `∘`, `+` and `∗`.

**Corollary 5.4 (blocks travel).** A term descends **as one object** for as
long as it skips. Nothing about it is decomposed, examined or rebuilt on the
way down.

**5.5 At the level a block cannot skip**, it either delegates (leaf) or
parenthesises (node), splitting along the two congruence directions of
Reading 3.2: parts acting on the head go to the **edge**, applied to the
primes; parts acting on the tail go **down**, applied to the subs; parts
relating the two become a case; the node rebuilds through Construction 3.3.

```
apply(H, V, d):
    if skip(H, V):           return d
    if V is a leaf:          return theory.apply_local(d, H)
    split H across the cut → (edge part, tail part, crossing part)
    for (P, S) in d:  emit ( apply(edge, V_h, P), apply(tail, V_t, S) )
                      and  crossing.case((P, S))          # §8
    canonicalise
```

**Proposition 5.6 (only `∘` factors).** A composition of parts with disjoint
supports is a tensor and factors into edge and tail. **A sum does not**:
`(h_e ∘ h_t) + (g_e ∘ g_t) ≠ (h_e + g_e) ∘ (h_t + g_t)`. A sum therefore
travels intact only while its summands skip *together*, and splits into
separate applications, joined at the level where they part company.

**5.7 The theory receives a term, not an opcode.** At a leaf, the whole
maximal local term is handed over (Definition 2.3). This is where the
per-theory normal form lives, and it is why `split_equiv` is *not* the
mechanism for local operations: a theory with a native operation must never
be made to split a code, act per piece and rejoin.

---

## 6. Saturation

Saturation is not a schedule bolted onto a finished semantics. It is what §5
already does when the travelling block happens to be a star, made explicit.
Its whole content is that **the split is derived from where the parts reach;
nothing is declared.**

**Definition 6.1 (the decomposition at a cut).** Write the term being
saturated as

```
H = (L + G + F + id)*
```

split by where each summand reaches relative to this cut:

| part | reaches | disposition |
|---|---|---|
| `F` | tail only | skips this level; belongs downstream |
| `L` | head only | local to this level's edge |
| `G` | both | a case at this cut |

`L`, `G`, `F` are each sums of compositions, and `id` sits in the sum so
each closure is reflexive.

**6.2 The schedule at a node.**

```
saturate(H, V, d):
    if V is a leaf:   return theory.apply_local(d, H)
    d := (F + id)*  on the subs                     # downstream first
    d := (L + id)*  on the primes                   # then local
    repeat until d is stable:
        for g in G:                                 # chaining
            d := (g + id)(d)                        # g is a case, §8
```

`(F + id)*` runs before the first `g` is ever touched, and the local
saturation runs too. The `G` loop then **chains** — each `g` applied to the
accumulated result in turn, rather than the whole sum iterated jointly.

**6.3 A crossing event is a case, and that is all it is.** Draft 2 gave
special rules here: a "Kronecker" `g` factoring as `l ∘ f` was thrown both
ways with re-saturation fused, guards of both members applied first, the
pair dropped if either was empty; a non-Kronecker `g` went to a separate
"general case" section. Both are one thing. `g` is a case (§8). Its query
program tests both ends before either update — which is not a scheduling
nicety but the ordinary shape of a case, since a case chooses what to ask
and in what order. Its reply is the morphism

```
down the tail:   (F + id)* ∘ f
on the edge:     (L + id)* ∘ l
```

when the factorisation exists, and an arbitrary node-sort morphism when it
does not. Re-saturation is fused into the reply because the reply is a
morphism and morphisms compose; nothing is left unsaturated behind an event.

The Kronecker/non-Kronecker distinction survives only as a **cost** regime
(§12), never as a case distinction in the semantics.

**6.4 The hierarchy is automatic.** The two child saturations are this same
procedure one level in, bottoming out where a theory closes its own local
term. No annotation, no user-supplied locality, no declared decomposition:
the split falls out of where the parts reach, so the procedure applies at
every sort of every shape.

**6.5 Why this is not round-based iteration.** Iterating `X ↦ d + h(X)` by
rounds rebuilds a fresh object each round, so every round misses the caches
and the cost is rounds × events × |X| in freshly built nodes. In 6.2 the
fixpoints sit *inside* the operator terms, so memoisation keys on
**saturated nodes**: a node is saturated once, ever. That is the whole
difference, and it is a difference in what is cacheable rather than in how
many operations are performed.

---

## 7. Queries

A query is what a case poses. It is the only construct that inspects data,
and it does nothing with what it finds — interpretation belongs to the case
that posed it (§8).

**Definition 7.1 (`split_equiv`).** For a criterion `E` and data `d` at sort
`V`,

```
split_equiv(V, d, E)  →  a finite map  { residual ↦ code }
```

partitioning `d` by the residual `E` leaves after this subtree has been
consumed. Codes are subcodes of `d`; residual codes are canonical and
interned; classes that agree are merged before any client sees them; empty
classes are absent. **The domain of the reply is the discovered alphabet.**

**7.2 The label is the residual, and `0` is adjoined to the codomain.**

Draft 2 had two notions — a *label*, being the value of a local expression,
and a *residual*, being what travels — and a special rule making booleans
not a label domain. Both go.

- **One notion.** A query returns the criterion **curried by what this
  subtree consumed**. When nothing remains to consume the residual is
  ground, and a ground residual is what draft 2 called a value. Currying is
  therefore not a separate mechanism; grouping classes by residual code and
  keying the memo on that code are the same act.
- **`0` is adjoined to the criterion's codomain**, not a value in it. A
  criterion is *free* to map part of its domain there and is not obliged to.
  That class is not represented, and computation stops on it. A filter is
  simply a criterion that junks; an `ite` is one that does not. There is no
  boolean exception, and no construct is owed to recover a negative class.
- **Criteria are total.** Partiality of a criterion as a function —
  out-of-range indices, division — is a typing question settled at
  declaration (§9), not a runtime junking. `0` therefore has exactly one
  meaning: this class is empty.

**7.3 The traversal, in three movements.**

**Down.** Travel to the first position the criterion mentions. A criterion
mentioning nothing in a subtree returns there in one class without
descending: locality is the distance-zero case and it is free.

**Across.** Meeting a coordinate grounds a subterm of the criterion and
renormalises, so the travelling criterion stays ground in what it has
consumed. Normalisation must apply *while a residual curries*, not only when
a client writes a criterion: currying is when residuals get their chance to
coincide. An applied operation therefore carries the builder that made it.

**Up.** Results federate; §7.6.

**Definition 7.4 (grounding order, and termination).** Each query **grounds
at least one subterm** of the criterion, innermost first. Since criteria are
finite terms, a query program terminates.

The measure is the *term*, not the partition and not the frontier. Two
consequences, both wanted:

- **A class need not shrink.** If the queried coordinate is already constant
  on the class, the reply has one class equal to the input — no refinement —
  and the program has still progressed, because it learned a value and
  grounded a subterm. A measure based on strict partition refinement would
  outlaw the cheapest and commonest query.
- **A position may be re-queried; a subterm may not.** Resolving
  `tab[tab[x]]` grounds `x` to `ℓ₁`, then grounds `tab[ℓ₁]` to `ℓ₂`, then
  addresses `tab[ℓ₂]`. Two queries land in the same leaf, at different
  addresses. Draft 2's rule "a consumed coordinate is never re-queried" is
  what term-grounding degenerates to when criteria are flat, and it is too
  strong for indirection, which is the interesting case.

Where a case constructs criteria adaptively rather than consuming a fixed
term, termination is not guaranteed by this measure and falls to the
contract of §8.6.

**Definition 7.5 (kernel and labelling).** A partition is stored as its
**kernel** — the canonical, interned tuple of pieces — plus a **labelling**
from residuals into it. Splitting the two is what lets partitions federate:
agreeing on the kernel is a pointer test, and disagreeing on the labelling
costs a finite table.

**7.6 The merge.** Two subqueries whose residual codes differ but whose
realised partitions agree are recognised on return and share one kernel,
keeping only their relabellings apart. Every partition federates, including
those that did not descend and those with a single class: not descending is
a locality optimisation, but not federating would let equal partitions carry
different kernels and the merge would stop being an equivalence.

This is where weak normalisation is repaid retroactively, and where the
**equivariant collapse** appears — not as a code path, but as what the merge
finds when the structure is there.

**Theorem 7.7 (canonicity of the quotient).** By the pieces alone: empty
classes are never represented; classes with equal parts are merged before
any client sees them; hence the discovered alphabet is the **coarsest
refinement compatible with `E` on `d`** — minimal, canonical, forced.
Per-query alphabet discovery inherits the uniqueness of Theorem 3.1
wholesale.

**7.8 Cost, structurally.** Per cut crossed between the criterion's support
and the queried cut, the traffic is

```
(# distinct realised normalised residuals — after merge: distinct kernels, plus finite tables)
  × distance (levels crossed)
  × class-code size in the theory
```

The first factor is a property of the criterion on the realised object; the
second is the shape's business, and locality is the distance-zero case; the
third is the theory's business. §8.5 adds a fourth factor, which is the
price of relating two queries at a cut. No a-priori bound can match a
per-query bill, since a-priori bounds cannot see the data.

---
**Ex2 (residue) — the curried walk.** Shape `(⟨b⟩,(J₁,(J₂,⟨c⟩)))` with `b, c`
ranging over a finite interval and the junk sorts unconstrained. Criterion
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

## 8. Cases

**Definition 8.1 (case).** A **case** at a node of sort `(V_h,V_t)` is a
function from the node's rectangles to morphisms at that sort,

```
case : ( ⟨r_e, P⟩ , ⟨r_t, S⟩ )  ⟼  a morphism at (V_h, V_t)
```

extended to data by linearity. It **owns its criteria**: it decides what to
ask, on which side, in which order, and may adapt what it asks next to what
it has learned. It is the only construct that opens a query and the only one
that closes one.

The returned morphism is **constant** in the sense that it depends on the
classes and not on anything else about the codes that realised them. That is
what makes the memo key `(r_e, r_t)` sound, and it is the sense in which a
case interprets *classes* rather than data.

**8.2 Linearity is the only law.**

```
case(x ∪ y) = case(x) ∪ case(y)          case(0) = 0
```

Four things follow, and they are the reason this is the law rather than one
law among several:

- The case is **determined by its action on rectangles**, so a node hands it
  `(P_i, S_i)` one at a time and sums. Theorem 3.1 supplies exactly that
  decomposition, for free.
- It is precisely the condition that the value **does not depend on which
  decomposition was used**, which is what licenses acting on *codes* rather
  than on elements — §0's "elements are never consulted", discharged here
  rather than assumed.
- It makes memoisation sound: the action on one class is independent of what
  other classes are present.
- It is the law the hom-sets already carry (pointed join semilattices,
  bilinear composition, `0`-strict). A case is therefore not a new kind of
  thing. It is an ordinary morphism, presented by a query program.

**Proposition 8.3 (split decorrelates, case re-tensors).** The reply may
always be taken to be a finite sum of tensors `Σ_{k≤n} h_k^e ⊗ h_k^t`,
because the correlation the criterion expressed has already been consumed by
the split: after classing, each side may be acted on independently and
relinked. This is a proposition about replies, not a constraint on the
return type — a case is free to return any node-sort morphism, including one
that is itself a `query ; case`, which is how refinement and indirection are
expressed with no additional construct.

The rank `n` is the tensor rank of §12 measured at one class pair:

| `n` | reading |
|---|---|
| `0` | the pair contributes nothing — the empty sum, exact (§4.7) |
| `1` | deterministic relink |
| `> 1` | genuine choice at this class pair |

Nondeterminism therefore lives in the **rank of the value**, never in the
matching: clause selection is deterministic, first match commits.
Canonicity is not maintained by hand — disjoint edge classes give **(D)**
for free, and two pairs whose replies agree give **(F)** on rebuild.

**8.4 Filters and assigns across a cut, defined.** Their expansions, at last
written down.

*Crossing guard* `a < b + c`, with `a` in the head and `b, c` in the tail:

```
query the edge on a          → residuals { (ℓ < b+c) ↦ P_ℓ }
per ℓ: query the tail on (ℓ < b+c)  → the positive class S_ℓ, or 0
reply: id ⊗ id  where S_ℓ ≠ 0;  the empty sum otherwise
```

The number of edge classes is the number of *distinct residual predicates*,
which is residual divergence (§12) — here bounded by the range of `a` unless
the interchange theory can coarsen `ℓ < b+c` into fewer classes.

*Crossing assign* `x ≔ y + z`, with `x` in the head:

```
query the tail on (y+z)      → residuals { v ↦ S_v }
reply: assign(x ≔ v) ⊗ id
```

The resulting edge parts may *overlap* rather than partition, which
Construction 3.3 absorbs, unioning the subs of pieces that coincide.

*Indirect assign* `tab[i] ≔ y + z`:

```
query on i                   → { ℓ_i ↦ … }
query on (y+z)               → { ℓ_v  ↦ … }
reply at (ℓ_i, ℓ_v): assign(tab[ℓ_i] ≔ ℓ_v)
```

This is the case that genuinely reaches the pair product, and it is the
reason a case must be a *function* and not a table: only the pairs that
actually meet are ever built.

**8.5 The dispositions, and the fourth cost factor.** A case relating two
sides may obtain what it needs in several ways, and they differ only in
cost:

| disposition | shape | cost |
|---|---|---|
| split the edge, specialise the tail | one query on the head, one tail application per class | `\|L_e\|` |
| split the tail, specialise the edge | the mirror image; edge parts may overlap | `\|L_t\|` |
| split both, match by residual | only agreeing pairs survive | `\|L\|`, the diagonal |
| split both, relate freely | every meeting pair | up to `\|L_e\| · \|L_t\|` |

These are Reading 3.2's two congruence directions seen from the operator
side: the node materialises one and defers the other. The **fourth cost
factor** of §7.8 is the number of realised meeting pairs, and it is the
price of leaving the Kronecker regime — zero there, since no case is called
at all (§12).

The pairing reads as a join over discovered keys: matching by residual is a
hash join, driving from one side is a nested loop, relating freely is a
cross join. Choosing among them is join method and join order selection
under cardinality estimates, and the cardinalities are the residual counts
the queries already computed. §15(iv).

**8.6 The contract.** A case is opaque by design — it interprets residual
types the engine has never heard of — so its obligations cannot be checked.
They are assume/guarantee, stated on the author, and are the exact analogue
of a theory certifying its own generators (§4.1) and of representability
being a per-element certificate (Definition 3.8). Discipline 4 applied to
operations.

The author guarantees:

1. **Linearity** — §8.2. A nonlinear case produces a wrong answer with no
   symptom, because the engine will apply it to a decomposition and sum.
2. **Class-determinacy** — the reply depends on the classes only. Violated,
   the memo returns a stale morphism.
3. **A well-founded query program** — §7.4's grounding order where criteria
   are fixed terms, some other well-founded order where they are constructed
   adaptively. Violated, the query program diverges.

The failure modes are the ordinary failure modes of the setting, not new
ones: a nonlinear case is a diagram whose edge points above its own node; a
non-well-founded query program is an integer that diverges. **The calculus
does not guarantee termination; it guarantees that non-termination is
attributable**, and the phenomenon ledger of §4 names the two sources — `∗`
for iteration, a case's query program for indirection.

**8.7 A case is a presentation format, not a semantic construct.**
Extensionally, `query ; case` adds nothing: every morphism at a node is
expressible as a case — split by the tautological criterion, read the answer
off per class. The construct is the **intensional presentation format** for
morphisms at a cut, and the whole content is *which presentations are
cheap*.

That gives a spectrum whose two ends are already in this document:

- the **tautological case** — maximal splitting, always correct, always
  expensive, and the instance that reproduces the structural decomposition
  itself (Definition 1.2, Theorem 3.5): **the calculus classifies against
  its own values, and its normal form is the special case of its own case
  construct**;
- the **trivial case** — no split, the operation factors, `case` is never
  called: §12's declared regime.

Everything real sits between, and the modelling art is finding the coarsest
case that still expresses the operation. This is the same principle as a
theory earning its keep by returning few structured codes rather than many
singletons (Definition 2.5), and as declared letter sets versus discovered
alphabets (§12). Three instances, one principle.

**8.8 Labels are scoped to a case.** A residual exists between a query and
the case that posed it, and nowhere else. Nothing labelled escapes into the
mainstream: outside a case there are only morphisms and data. Two
consequences:

- The calculus **genuinely never interprets a residual**, and this is now a
  structural fact rather than a stated intention.
- Both ends of a criterion are inside one case, so the interchange theory
  (Definition 2.7) is asserted *by the case author*, at one place, and there
  is no global agreement to maintain between unrelated theories.

**8.9 Label-based synchronisation, and the expressiveness claim.** Read §8.4
as a *synchronisation discipline*: the parts of a shape are arbitrary,
mutually ignorant theories, and a crossing operation is a rendezvous between
them conducted entirely in residuals. One side offers a residual; the other
answers the query indexed by it; the case relinks. Neither side needs to
know the other's algebra — only that the interchange theory is shared, and
in the degenerate case that is only equality.

That is what lets sorts be **mixed freely**: a Boolean block, a bounded
integer field, a word algebra, and a nested diagram may all sit in one shape
and still interact, because interaction is mediated by discovered residuals
rather than by a common representation. Declared synchronisation alphabets
are the familiar version of this; what is added here is that **the alphabet
is discovered per query** — minimal, canonical, and forced by Theorem 7.7
rather than chosen by a modeller. Systems whose every event factors never
invoke it at all (§12); systems that need genuine correlation pay for
exactly the correlation they use.

---

## 9. The surface

The concrete syntax borrows SMT's: sorts and theories declared, terms typed,
new theories added by declaration rather than by extending the grammar. This
is not cosmetic. Theory combination is what §8 does one level down, so
surface and semantics share a lineage.

**9.1 Declarations.**

```
(declare-leaf Fork (enum free tk))
(declare-leaf St   (enum T HL E))
(declare-leaf Tab  (array Int Int))
(declare-interchange LIA)

(declare-shape Ph  (pair (F Fork) (S St)))
(declare-shape V   (pair (pair Ph Ph) (pair Ph Ph)))
```

**9.2 Operations are two-phase.** Enabling and acting are the two things a
case separates, so the surface has exactly two slots and the order cannot be
written wrong:

```
(define-op takeL
  :when (and (= S T) (= F free))
  :do   (assign (S HL) (F tk)))
```

Whether this compiles to a local term or to a query/case bracket is decided
by where the mentioned positions sit (§4.2), and the modeller is not told
which.

**9.3 Cases and discovered labels.** A case names its criterion and its
clauses; a default is **required**, because the alphabet is discovered and
the modeller cannot enumerate it:

```
(define-case shift
  :on   (mod (+ b c) 3)
  :when ((0 opA) (1 opB) (2 opC))
  :else junk)                        ; junk | id | an operation
```

Clauses are tried in order, first match commits (§8.3).

**9.4 The surface never mentions cuts.** A criterion is written over
positions; whether it crosses a cut, and which disposition of §8.5 is used
to resolve it, is the engine's. Two reasons, and both matter more than
convenience: the choice is a cost decision that only the engine can price,
and a shape is refactorable — the same behaviour has many trees, and a case
written against a cut would not survive re-shaping.

Names are likewise the engine's problem. A client manipulates `X` that is in
fact reached at `a.b.c.X`; symbols are resolved to paths at declaration and
re-indexed over the local shape domain at use, so what a theory receives is
always currified (§2.6) and never a global name.

**9.5 The invoice.** `(bill)` reports the cost model of §7.8 and §8.5
directly, not ad hoc counters:

| counter | factor |
|---|---|
| distinct realised residuals per query | §7.8, first |
| levels crossed | §7.8, second |
| leaf calls and class-code sizes | §7.8, third |
| meeting pairs per cut | §8.5, fourth |
| junked classes | diagnostic only — profiling, not algebra (§4.7) |

`(size X)` reports per-node prime counts, which is the §14 measure.

---

## 10. Theories, worked

**10.1 Enumerated.** An explicit finite carrier, coded by subsets. Tier B
trivially; `split_equiv` by enumeration. Its purpose is to isolate
calculus-level questions from theory-level ones: everything it does is
obviously correct, so a discrepancy is the calculus's. It is also the
baseline every other theory should beat, since enumeration is the one thing
a theory can do badly.

**10.2 Boolean blocks (BDD).** A block of Boolean bits, coded by binary
decision diagrams over a **currified** variable set (§2.6): one set of
variables ever, indexed from the terminal upward, shared by every position.
Codes are canonical and hashable by construction, and all four tiers come
free.

- **Local terms are relational products.** The theory receives a maximal
  local term (Definition 2.3) and fuses *all* of it: composition becomes
  relational composition, `+` becomes disjunction of relations, `∗` becomes
  transitive closure — one relation over interleaved current/next variables,
  applied once. Interleaving is the only layout for which the product is
  efficient.
- **`split_equiv` has several query forms.** By a *predicate*: one meet. By
  a *cube* naming which variables to split on: the realised valuations of
  that cube, indexed by those valuations. By a *formula over the block and
  beyond*: the distinct co-factors, which is Theorem 3.1 one level down,
  performed by the theory.
- **Consequence of currification.** A criterion relating two positions is
  never one diagram of this theory, since both positions use the same
  variables. It is a local query plus a residual that travels — §2.6, and
  the reason §7 and §8 are structured as they are.

**10.3 Bounded integer fields (FDD).** Finite domains encoded over the same
currified Boolean variables: a field is a group of bits, `field = value` is
a code, and everything in 10.2 carries over unchanged. `split_equiv` by a
field is the cube form, and its residuals are ground integers. A bounded
place/transition system is then one field per place, transitions as
relational products, every event factoring, and §6 applies with no case ever
called.

**10.4 Linear integer arithmetic, and arrays.** The working interchange
theory (Definition 2.7) and its companion. Criteria are LIA terms over
coordinates in different leaves; `split_equiv` by a LIA term returns classes
indexed by ground values; `tab[i]` is a select whose index is resolved by an
outer query and whose result may itself index another select.

This pair is where indirection lives, and therefore where §7.4's
term-grounding measure earns its keep: `tab[tab[x]]` grounds `x`, then
`tab[ℓ₁]`, then addresses `tab[ℓ₂]`, three queries and a finite term. The
termination argument for nested selects in array decision procedures
instantiates on the finite set of index terms occurring in the formula,
innermost first, which is the same argument; the coincidence is worth
checking against the primary sources before it becomes a citation.

**10.5 Recognisable sets.** The paradigm infinite theory: minimal
recognisers as canonical codes, tier B, concatenation and quotient as
exported maps. Every touched class is finitely coded though the algebra is
infinite — discipline 4, exemplified.

---

## 11. Extension versus intension

**Theorem 11.1 (local constancy).** The graph of a total map is a finite
union of rectangles iff the map has finite image. **Corollary:** the diagonal
of an infinite carrier has no finite rectangle form — the extensional
encoding cannot express *no-op*, while the calculus has `id` free twice
(empty composition; a self-assign). Over finite carriers every relation of
out-degree `d` is a sum of `d` guarded assigns, so the calculus subsumes
extensional encodings, strictly at infinite theories. Guards, by contrast,
*are* data: the asymmetry of the local basis — selectors reduce to data,
complete morphisms do not — is this theorem speaking.

**Definition 11.2 (trace).** The trace of `h` on explored `X` is
`graph(h) ∩ (X × ⟦V⟧)`. On any finite fragment the restriction has finite
image, so the trace is rectangle-representable *precisely and only per-run*:
memoisation is the runtime construction of the finite-index certificate. The
extensional encoding is what one gets by paying for that certificate before
the run.

**11.3 The same statement one level up.** A case is intensional — a function
that interprets classes — and its extensional table over residual pairs is
exactly its trace: accumulated as the memo of pairs that actually met, never
built in advance. Building the table eagerly would cost the pair product
(§8.5, fourth row) for a case that may touch a diagonal's worth of it. So
Definition 11.2 holds for operations and for cases by one argument, and in
both the answer is the same: intension is primary, extension is the per-run
certificate.

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

## 12. Cuts, ranks, and the two regimes

Across a fixed cut, data has **section rank** — the number of primes, forced
by Theorem 3.1. Operations have **tensor rank** — the least `n` with
`h = Σ_{m≤n} ⊗_ℓ h_{m,ℓ}`. Tensors of local terms are local, so a
block-presented system stays in normal form; block operations along a cut
form a **non-unital ideal**, missing exactly the diagonal (Theorem 11.1).

Two regimes for the interaction alphabet across a cut:

- **Declared:** a finite letter set fixed a priori — an upper bound on tensor
  rank, constraining only the interface. Choice is externalised to the
  letters; within a letter, deterministic.
- **Discovered:** the per-query minimal alphabet of §7 — canonical,
  coarsest, priced by transport.

**The declared regime is exactly the fragment in which no query is ever
posed across a cut, and it is exactly the fragment in which every crossing
operation factors**: every event a tensor of local actions, §6 applying with
every `g`'s reply of rank one and no split at all. Place/transition systems
live there permanently, under *any* decomposition. That is why they saturate
so well, and why a criterion travelling in such a system would be a symptom
of bad encoding rather than of expressive power.

The discovered regime removes the bound at the stated cost. The parallel is
precise: *declared letter sets are to discovered alphabets as declared leaf
partitions are to discovered primes* — upper bounds a modeller accepts in
exchange for never transporting.

**12.1 A conjectured identification.** The condition under which a query
returns a single class is a property of the theories involved, not of the
model: it is the condition under which a criterion determines one value
rather than a disjunction of them. Theory combination calls the
corresponding property **convexity**, and calls its failure the reason case
splitting is needed at all. If the identification holds, then

```
declared regime  =  no case called  =  every crossing criterion convex
```

and §12's dichotomy stops being an observation about modelling style and
becomes a theory-level property with per-theory answers already worked out:
LIA convex, LIA with arrays not, and `tab[tab[x]]` the reason. That would
*explain*, rather than merely note, why place/transition systems saturate
perfectly and why array indirection is where one begins to pay. Stated as a
conjecture because theory combination reasons about satisfiability and this
calculus computes partitions of data; the correspondence is architectural
until proved otherwise. §15(iii).

One invariant governs both ranks and the §7.8 bill: **residual divergence** —
how fast the residuals of realised criteria separate as the head varies.
On Ex1 every protocol-relevant criterion's residual depends on the head only
through the two boundary forks — at most four residuals, ever, at any size.
On Ex2 residuals separate only up to residue — three, at any range. When
residuals separate as fast as the carrier — two coordinates constrained
equal across a cut is the canonical case — no mechanism helps, and §14 says
the one thing this document says about it.

---

## 13. The streaming factorisation

Call a presentation a **finitary stream** if it is given by finite control
and finitely many registers valued in declared sorts, consuming the frontier
left to right, each step emitting local actions parameterised by the letter
read and the register contents, with admissible register updates. Two
hypotheses, both load-bearing: the stream's transition *tests* are
admissible criteria, and the configuration is *declared* — control finite,
every register sort an existing sort.

**Theorem 13.1 (factorisation; proof owed, §15(v)).** Every shape-respecting
finitary stream satisfying both is extensionally equal to a term of the
calculus over local terms, cases, tensor-with-constants, structural maps,
`+` and `∘` — with, over finite shapes, **no `∗` at shape level**: iteration
survives only inside sequence-structured theories. *Star-free over the
shape; stars only in the alphabets.*

**Construction.** Thread the configuration as data; branch only on control.
Enlarge the sort by a configuration component, initialise by
tensor-with-constant, and let the stream become a straight-line composition,
one term per frontier leaf, associated to the shape tree by structural maps.
Each per-letter step splits along the phenomenon ledger: the control
successor has finite image, hence is blockwise by Theorem 11.1 — a case
whose discovered alphabet is bounded by it; the register and emission
updates are possibly infinite-image but single-valued — one assign per
branch, inside the case's reply. Finally marginalise the configuration away.
Finite shape means finitely many steps, hence no `∗` at shape level.

**Conjecture 13.2 (adjoined configuration).** A stream with genuine cross-cut
register flow is inexpressible over the original shape alone: the detour
through the enlarged sort is unavoidable, and the *least* adjoined
configuration sufficient for a behaviour is an invariant of it —
conjecturally the tensor rank of §12 in another gauge.

---

## 14. Remark on compression, at arm's length

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
coordinate occupies its own place in a global order. By Corollary 3.6 the
same argument covers internal nodes, since a nested diagram is a code like
any other.

This document defines the calculus and prices its operations. Whether a
given semantics admits a shape and theory encoding under which it compresses
is the modeller's problem, and this document deliberately says nothing more
about it.

---

## 15. Obligations and seeds

In order of load-bearing-ness.

**(i) The transport lemma.** One lemma, two faces. *Mechanism face:* the
curried residual traversal of §7.3 computes the quotient — soundness of
currying, of code-level deduplication, and of the retroactive merge, with
the three-factor bill of §7.8 as the proved cost, and the induction on
**term grounding** (Definition 7.4) rather than on frontier position.
*Staging face:* per-node continuation congruences, composed along the spine,
reconstruct the full congruence of the classified object — the formal
content of "a representation is the congruence tower" (Reading 3.2). The bet
is that these are one induction read in two directions.

**(ii) The rewriting system.** §4.8 states the shape of the answer without
giving it: confluent rules over commutativity and relative support,
canonical representatives for sums of terms modulo the semiring laws, the
merge laws, and independence. Prove confluence, and prove that the schedule
of §6.2 is expressible as its rules — the claim that the normal-form
question and the scheduling question are one question. Expected hard,
expected central.

**(iii) The convexity identification.** §12.1. Prove or refute that "no case
is called across this cut" coincides with convexity of the crossing criteria
in the participating theories. If it holds, the declared/discovered
dichotomy inherits a literature and per-theory answers; if it fails, the
manner of failure is itself informative about what the partition setting
demands beyond the satisfiability setting. Newly promoted, because it is the
one place where an outside body of results would attach.

**(iv) The direction rule.** §8.5 lists the dispositions and prices them but
does not choose. The choice is join method and join order selection under
cardinality estimates that the queries already produce; import the
machinery, or state a heuristic with a regret bound. This is the last place
where the calculus defers to a modeller inside an operation rather than at
the interface — and §9.4 has already promised the modeller it will not.

**(v) The factorisation proof.** Complete Theorem 13.1 by the threading
construction, with cost transparency; then attack Conjecture 13.2.

**(vi) The generated category, stated once.** §4 defines operations as terms
with a code action and demotes the kernel to the extensional shadow. State
this with nothing informal left: which terms, which actions, which quotient,
and the closure argument that evaluability is preserved by `∘`, `+`, `∗` —
including the case construct, whose evaluability is contractual (§8.6) and
must be stated as an assumption of the theorem rather than smuggled.
Then `hom(1,(V_h,V_t))` as a tensor of pointed modules with Theorem 3.1 as
its canonical-basis theorem, making the qualitative-primes gauge functorial
rather than legislated.

**Discharged since draft 2.** The *dynamic footprint* obligation is gone:
§5.1 defines skip from `id` and §5.2 makes static support one oracle among
several, with resolved indirection supplying the exact reach per realised
class. Nothing further is owed there.
