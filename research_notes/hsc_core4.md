# Hierarchical Shape Calculus

*Working draft 4. Standalone: every object is defined from nothing, and the
document owes nothing. It is written to be the seed of its own iteration — a
reader holding only this text should be able to continue the work. §16 lists
where to push.*

*What moved since draft 3. A **fifth discipline**: precision moves the bill,
never the meaning — promoted from a property of theory normalisation to a
global law that also governs the skip oracles, the side conditions of the
rewriting system, and the timing of the merge; its consequence is that every
canonicity claim in this document is extensional, and syntactic normal forms
are schedules. **Certificates and durability** are now a section, not a
remark: memoisation was load-bearing in draft 3 ("a node is saturated once,
ever") while its lifetime was nobody's business; the certificate store now
belongs to the cost model, with attributable invalidation as its one law.
The **degeneration axes** are named: the calculus now specifies its own
special cases — absent interaction, enumerated theories, flat shape — and
requires each boundary to be recognizable and each specialization to be
free, so that the classical flat diagram calculus is the *value of this
calculus at the origin* rather than a competitor to it. The **convexity
identification of draft 3 (§12.1) is refuted as stated** — two stress cases
below — and replaced by value-blindness, with theory convexity surviving as
a conjectured bound on coarsening. The **case contract** gains instruments:
every obligation has a finite witness, and the calculus requires the
referee, not the proof. Sorts, theories, the decomposition theorem, skip
from `id`, the saturation schedule, and the query/case bracket are carried
from draft 3 with compression where they are settled. Coefficient
semirings, multi-terminal values and inverse images remain out of scope.*

---

## 0. The stance

The object of study is the **classification of structured words**. A word
is a tuple shaped by a tree; a classification assigns to each word a class;
the default classification — the semantics when nothing else is said — is
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
  *compute* anything.

This is not setoid mathematics: the congruence is relative to the
classified object and the cut — discovered, not ambient — and operations
are not required to respect it. Well-definedness is not assumed; it is
*restored*, by re-canonicalisation after every action.

**Five disciplines** govern everything, and are stated before any
definition because every definition obeys them.

1. **Typing by adjunction.** Two constants are never citizens. `0` is
   *adjoined*: carriers are extended by a free point, so `0` is a legal
   *answer* and never a legal *letter* — nothing canonical stores,
   traverses, or points to it. `id` is *free*: the empty composition, never
   represented. Extending pointed definitions by adjoined elements, instead
   of admitting those elements into the definitions, is the single move
   that eliminates the traditional degenerate cases.

2. **Vision by staging.** Every rule accesses a sort through one of two
   windows: the *partition window* (construct equivalence classes: meets,
   relative differences, emptiness) or the *naming window* (compare
   canonical codes: equality, zero test). Each node materialises exactly
   one congruence, its own cut's, and defers all others downward.

3. **Skip is `id`-propagation.** An operation whose restriction to a
   subtree is `id` transmits nothing there, because `id` is free and free
   things are not represented. That is the whole of skipping (§5).

4. **Finiteness per element.** No carrier, alphabet or algebra is required
   finite. What is required is that each *represented class* admit finite
   canonical codes and that each *run* of an operation touch finitely many
   codes. Global guarantees are replaced by per-element and per-run
   certificates, discovered rather than declared.

5. **Precision moves the bill, never the meaning.** Wherever the calculus
   consults an oracle of variable strength — a theory's normalisation of
   residuals (§2), a skip oracle (§5.2), a side condition of the rewriting
   system (§4.8), the timing of the merge (§7.6), the retention of a
   certificate (§10) — a weaker answer may cost more and may never change
   the result. Soundness obligations are owed under *every* sound oracle;
   canonicity claims are extensional; anything whose presence or absence
   would alter meaning is not an oracle but a definition, and must be
   stated as one.

**Lineage, stated plainly.** The structural normal form generalises the
compressed, trimmed decision-diagram forms over variable trees; "primes"
and "subs" are used in that tradition's sense, with the shape tree in the
role of the vtree. The continuation congruence is the classical right
congruence of word classification. The local layer is guarded-command
territory. The cross-cut layer is theory-combination territory: a mixed
criterion purified by the shape into per-theory parts that exchange only
values in a shared sort.

The claimed delta, four items:

- **Currified naming.** A code is relative to its position and therefore
  shared across isomorphic positions — which no fixed global variable
  order can express (§2.6, §15).
- **Skip as `id`-propagation**, from which operations that travel as
  opaque blocks, and hierarchical saturation, are *derived* rather than
  retrofitted (§5, §6).
- **The query/case bracket.** A scoped, author-certified, linear
  presentation of morphisms at a cut, whose vocabulary is discovered
  residuals rather than a declared synchronisation alphabet (§7, §8).
- **The priced degenerations.** The classical flat calculi are values of
  this one at named, recognizable, freely-specializable points (§11); the
  general mechanism is never a tax on the special cases.

**Out of scope, deliberately.** Weighted and multi-terminal readings: a
classification whose codomain is a semiring is a *classifier*, and a
theory that represents such classifications compactly is a *leaf sort*.
Inverse images, absolute or relative: backward reasoning needs a reachable
set to move within, which is a different problem — a derived layer over
the calculus that takes that set as an operand, never a method of its
morphisms.

---

## 1. Sorts and shapes

**Definition 1.1.** Fix a family of imported theories. **Shapes**:

```
V ::= 1 | ⟨A⟩ | (V_h , V_t)
```

with denotations `⟦1⟧ = {•}`, `⟦⟨A⟩⟧ = U_A`,
`⟦(V_h,V_t)⟧ = ⟦V_h⟧ × ⟦V_t⟧`. A **word** of sort `V` is an element of
`⟦V⟧`; the **frontier** of `V` (leaves, left to right) exhibits words as
tuples read in order, bracketed statically by the tree. A **position** is
a path from the root.

Three structural commitments:

- **No names.** Positions are paths; ordered trees are rigid; two equal
  subterms are one object under hashing. Naming is a surface decoration
  erased at declaration time (§12). §2.6 pushes this into the theories,
  where it becomes load-bearing.
- **No associativity.** `(V₁,(V₂,V₃))` and `((V₁,V₂),V₃)` are distinct
  sorts with a *computed* isomorphism between them. Refusing the quotient
  by associativity is what "hierarchical" means here.
- **The unit sort is mono-sorted.** `1` exists at one shape only and has
  no frontier, so no position addresses it.

**Definition 1.2.** At `1` sit, as one object: the **terminal value**; the
**default classification** (acceptance); and the base case of the
**default classifier**, the continuation, whose residual once every
coordinate is consumed *is* that value. The third identity is why
classifying against the continuation reproduces the normal form (§8.7).

---
**Ex1 (philosophers) — statics.** `n` philosophers in a ring, `n` a power
of two. Fork `F_i` sits between philosopher `i` and `i+1`; philosopher `i`
uses `F_i` (left) and `F_{i−1}` (right). Each philosopher is one leaf
carrying its local state; the shape is balanced over the philosophers, so
**every cut is crossed by exactly two forks**. The point of the example:
its representations stay linear in the number of philosophers while its
state space is exponential.

---

## 2. Theories

A leaf imports an external domain through a finite window; the window, not
the domain, is what the calculus sees.

**Definition 2.1 (support algebra).** A family `ℛ ⊆ 2^U` containing `∅`,
with canonical finite codes for its elements. Cumulative tiers:

- **E**: codes; decidable equality; decidable emptiness.
- **J**: E + finite joins.
- **G**: J + finite meets and **relative** differences `A ∖ B` — a
  generalised Boolean algebra: relatively complemented, distributive,
  least element, **no top required**.
- **B**: G + top and complement.

Tier G is the working tier. A theory without a top is legal and expected;
no construction below forms one.

**Definition 2.2 (exported maps).** A theory may export maps `f : U → U'`
as raw material for assigns, by exporting the pushforward `f_* : ℛ → ℛ'`
on codes. **No preimage is exported, absolute or relative, and none is
ever requested.**

**Definition 2.3 (local terms).** A theory is asked not for one map at a
time but for a **maximal local term**: the whole term — guards, assigns,
composition, sum, star closure — whose support lies in this leaf, handed
over for the theory to interpret however it likes. The normal form for a
local term is per theory; the calculus delivers the term and does not
prescribe the fusion. Splitting a code, acting per piece and re-joining is
what a theory with a native operation must never be made to do.

**Definition 2.4 (normalisation strength).** A theory exports canonical
codes for the closed terms of its operation and criterion languages — the
expressions that travel as residuals (§7). The strength of this
normalisation is a declared property and an instance of discipline 5:
undetected equalities cause redundant subqueries, repaired retroactively
by the merge of §7.6; they never cause wrong answers.

**Definition 2.5 (`split_equiv`).** The theory's partition primitive:
partition a code by the residual of a criterion, returning the realised
classes indexed by residual. Contract and typing in §7.1. Enumerating the
carrier is a legal implementation and the honest one for a small finite
carrier; a theory earns its keep by returning few structured codes where
enumeration would return many singletons, and that ratio is the one cost
factor decided entirely by the theory.

**2.6 Currification.** A theory's codes are **relative to a position, not
absolute**. One set of variables — one indexing, whatever the theory's
internal representation — is ever instantiated, indexed from the terminal
upward, and every position using that theory uses the same one. A leaf of
width `k` uses the first `k`.

The consequence: *a code built for one position is the code for every
other position with the same local structure*. Four philosophers in the
same local state are one code, and the representation never learns that
there are four of them. Internalisation (Theorem 3.5) is the same
phenomenon for the diagram theory, where hash-consing supplies
position-relativity for free.

The price, stated honestly: a criterion relating coordinates in two
different positions **cannot be a single object of the theory** — both
would use the same names. Cross-position criteria are therefore
necessarily structured by the shape, exactly as data is: a local query,
then a residual that travels (§7.3). This is why §7 and §8 exist.

**Definition 2.7 (interchange theory).** A criterion whose subterms are
evaluated in different theories needs a sort in which those theories
agree. That sort is declared, and the theory over it is the **interchange
theory**. Two fixed points of its role:

- **Equality is always available.** The degenerate interchange theory —
  labels compared only for equality — exists for any pair of theories and
  supports pure rendezvous.
- **Linear integer arithmetic is the working one.** Index expressions,
  arithmetic guards across a cut, and array addressing live there.

The calculus never interprets a residual. But the theories at the two ends
of a criterion must interpret it identically, and the interchange theory
is where that agreement is declared. Types are declared; alphabets are
discovered.

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
`X(x,–) = X(x',–)`. Theorem 3.1 says: the primes are its classes,
constructed through the partition window; the subs are the class *names*,
compared through the naming window. Dually `X` induces a congruence on the
tail coordinate; the node does not materialise it — the subs' own
decompositions do, level by level. **A representation is the
classification's congruence tower, materialised one cut per node and
deferred elsewhere.** The two directions recur as the dispositions of a
case (§8.5).

**Construction 3.3 (compilation).** From any finite rectangle expression,
insert incrementally into a maintained disjoint partition: for each
rectangle, meet against the cells already present, carve the overlaps out
by *relative* difference, join the subs where they coincide, and finally
group by sub. The one cell whose expression would need a top — the
all-negative pattern — has section `0` and is deleted by the pointed
discipline before it is written: non-unital heads and partial
decompositions each fill exactly the hole the other leaves. Every prime
ever written is a nonempty meet or a nonempty relative difference of
primes already present.

**The degeneracy ledger.**

| rule | kills | licensed by | consumes |
|---|---|---|---|
| no zero subs | padding | `p ⊗ 0 = 0` (smash) | tail zero test |
| no zero primes | phantom classes | fibres of nonzero sections are nonempty | head emptiness — the one semantic decision canonicity leans on |
| (D) | overlap | — | head disjointness |
| (F) | refinement | distributivity | tail equality |

**Definition 3.4 (diagrams).** `Diag(1)` = the terminal; `Diag(⟨A⟩)` =
canonical theory codes; `Diag(V_h,V_t)` = the adjoined `0`, or a finite
nonempty set `{(P_i,S_i)}` satisfying (D) and (F). Arcs draw from the
unpointed carrier; `0` enters only as an answer. Hashing makes equality a
pointer test and `0` an absence. Zero-freeness maintains itself: smash
makes `0` an absence, absence makes emptiness at every composite sort a
pointer test, and free emptiness makes the invariant free to preserve.

**Theorem 3.5 (internalisation; hierarchy as closure).** For any shape
whose head-position subtrees are hereditarily tier G, the diagrams over it
form an admissible support algebra (tier G; tier B if all leaves are B),
coded by their hashes. Hence `⟨Diag(V)⟩` is a legal import: **the calculus
eats its own output, and "hierarchical" is this closure property.** There
is one diagram type, not two — a prime over a composite head *is* a
diagram over that head. The induced typing law: head subtrees uniformly G,
tail spines pay-as-you-go — *classes need differences; names need a hash.*

**Corollary 3.6 (internalisation carries operations).** A theory must
export `split_equiv`. Diagrams are a theory. Therefore **diagrams must
export `split_equiv`**, and §8's construction is that export. There is no
"leaf case" and "node case" — one construct, whose leaf instance is
delegated and whose node instance is §8.

**Reading 3.7 (the alphabet tower).** Along the frontier, a diagram is a
trim partial recogniser whose letters are *computed*: the letters at a
node are its primes; level k's alphabet is level (k+1)'s state set. A leaf
is the pair *(recogniser, continuation)* with the recogniser imported — a
black box; an internal head is the recogniser computed — a white box;
internalisation says a white box can be re-boxed black.

**Definition 3.8 (representable).** `X` is representable over `V` iff at
every cut, hereditarily, its continuation congruence has finite index and
its classes are expressible in the head's algebra. Representability is the
only finiteness demanded of *data*; §8.6 states the one demanded of
*operations*.

---

## 4. Operations: the basis

**Definition 4.1 (the basis).** Operations are terms:

```
H ::= id                       -- free; never represented
    | local(t)                 -- a maximal local term, handed whole to a theory
    | query(E) ; case          -- the crossing bracket (§7, §8)
    | H ∘ H  |  H + H  |  H*
```

A term **acts on codes**, and each action is followed by
re-canonicalisation. Hom-sets are pointed join semilattices; composition
is bilinear and `0`-strict; a term applied to data may answer `0` —
deadlock — and nothing is ever undefined.

**4.2 Filters and assigns are not generators.** They are **primitive
inside `local`** — a guard or assign whose support lies in one leaf is a
subterm of the maximal local term the theory fuses — and **derived across
a cut**: a guard or assign relating coordinates in different subtrees
*expands* into `query ; case`, and that expansion is its definition
(§8.4).

**Definition 4.3 (the extensional shadow).** Where theories code
singletons, a term determines a pointwise kernel. **Extensional equality**
is agreement of kernels on all words — the court in which identities are
judged, never the mechanism of evaluation.

**The two faces.** A support element `e ∈ ℛ(V)` types two ways: as a
**point** ("these words") and as a **sub-identity** ("keep these words").
One code, two typings; the second carries `∩` to `∘` and `∪` to `+`. The
faces interconvert only where a top exists, and most of the calculus never
buys that bridge.

**Theorem 4.4 (the guard algebra).** The sub-identity interval `[0, id_V]`
is a **unital Boolean algebra even over non-unital supports**: its top is
`id`, which is free, and complement is `(¬h)(w) = {w}` iff `h(w) = 0` —
computed on data as a relative difference, tier G, no `⊤` anywhere. The
morphism algebra strictly outruns the data algebra: a derived predicate
such as *deadlocked* — the complement of the union of all enabling
guards — is computable against the data present, with no top in sight.

**4.5 `ite` is not a construct.** A criterion is free to name its negative
class rather than junk it (§7.2), and the negative class is codeable at
tier G by relative difference against the data present. A **filter** is a
case whose criterion junks the negative class; an **`ite`** is a case
whose criterion names it. Same query, same machinery, different criterion.

**Proposition 4.6.** The operator semiring is **zerosumfree**
(`h + h' = 0 ⟹ h = h' = 0`: no cancellation, hence monotone fixpoints) but
**not** zero-divisor-free (disjoint guards compose to `0` with both
nonzero). Negation, banished from the semiring, survives exactly on the
idempotent interval, where the free unit gives it something to be relative
to.

**4.7 Deadlock is exact.** A class pair contributing nothing contributes
the **empty sum**, a value of the same construction that produces every
other. No run is ever an under-approximation on account of a junked class,
and no report is owed. (A modeller wanting to know a guard never fired
reads a counter in the invoice, §12.5 — profiling, not algebra.)

**4.8 On normal forms.** A canonical word is the wrong shape of answer.
The right one is a **confluent rewriting system** whose rules consult
commutativity and the *relative support* of operands to decide reordering,
merging, and refusal to merge. Merging two single-coordinate guards into a
two-coordinate guard is anti-optimal: the merged guard must now cross a
cut where separately neither would. The reordering such a system enacts is
the reordering a schedule needs — the normal-form question and the
scheduling question are one question seen twice.

Discipline 5 binds this system: its side conditions (supports,
commutation) are answered by oracles of variable precision, so **rules are
owed sound under every sound oracle**; two implementations of different
strength reach extensionally equal results at different cost, and the
"normal form" is per-toolchain. Confluence is claimed up to extensional
equality, never up to syntax. §16(ii).

**Structural maps.** Unitors; the computed associator; swap; theory
isomorphisms; marginalisation (tail joins, tier J); duplication. These are
the sort-changing citizens; they exist only along legitimate shape maps.

**The phenomenon ledger.** Each construct owns one phenomenon and provably
introduces no other:

| construct | owns |
|---|---|
| `id` | nothing — the free unit, and the content of skip |
| `local` | whatever the theory owns: local partiality, movement, choice |
| `query` | class discovery — the alphabet, and quantification over a support |
| `case` | interpretation of classes, and re-tensoring after decorrelation |
| `∘` | sequencing |
| `+` | nondeterminism across levels |
| `∗` | unboundedness from iteration |
| a case's query program | unboundedness from indirection — contract, §8.6 |

---

## 5. Skip, and blocks that travel

*Orthogonal to, and logically preceding, saturation: how a term reaches
the level where it acts.*

**Definition 5.1 (skip).** A term `H` **skips** a subtree `V` iff its
restriction to `V` is `id`:

```
skip(H, V)   ⟺   H|_V = id
```

Nothing more. `id` is free, so a skipped subtree receives nothing, and the
level above it transmits `H` unchanged.

**5.2 How the skip question is answered.** `skip` is a *query posed to the
term*, and several oracles may answer it, in increasing precision —
discipline 5 in its purest instance:

- **Static support.** The set of positions the term's syntax mentions.
  Cheap, sound, adequate exactly while every operation's reach is static.
- **Resolved indirection.** After a query grounds an index, the residual's
  reach is exact: `tab[i]` mentions all of `tab` statically, but per
  discovered class `i = ℓ` the residual mentions the single position
  `tab[ℓ]`. Dynamic reach is the union of residual reaches over *realised*
  classes: one pays for the cells the data actually visits.
- **Theory declaration.** A theory that knows a local term is the identity
  on part of its own state may say so.

**Proposition 5.3 (skip is compositional, uniformly).** A term skips a
subtree exactly when every one of its parts does, and the rule is the same
for `∘`, `+` and `∗`.

**Corollary 5.4 (blocks travel).** A term descends **as one object** for
as long as it skips. Nothing about it is decomposed, examined or rebuilt
on the way down.

**5.5 At the level a block cannot skip**, it either delegates (leaf) or
parenthesises (node), splitting along the two congruence directions of
Reading 3.2: parts acting on the head go to the **edge**, applied to the
primes; parts acting on the tail go **down**, applied to the subs; parts
relating the two become a case; the node rebuilds through Construction
3.3.

```
apply(H, V, d):
    if skip(H, V):           return d
    if V is a leaf:          return theory.apply_local(d, H)
    split H across the cut → (edge part, tail part, crossing part)
    for (P, S) in d:  emit ( apply(edge, V_h, P), apply(tail, V_t, S) )
                      and  crossing.case((P, S))          # §8
    canonicalise
```

**Proposition 5.6 (only `∘` factors).** A composition of parts with
disjoint supports is a tensor and factors into edge and tail. **A sum does
not**: a sum travels intact only while its summands skip *together*, and
splits into separate applications joined at the level where they part
company.

**5.7 The theory receives a term, not an opcode.** At a leaf the whole
maximal local term is handed over (Definition 2.3). This is where the
per-theory normal form lives, and why `split_equiv` is *not* the mechanism
for local operations.

---

## 6. Saturation

Saturation is not a schedule bolted onto a finished semantics. It is what
§5 already does when the travelling block happens to be a star, made
explicit. Its whole content: **the split is derived from where the parts
reach; nothing is declared.**

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

`id` sits in the sum so each closure is reflexive.

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

`(F + id)*` runs before the first `g` is touched; the `G` loop **chains**
— each `g` applied to the accumulated result in turn.

**6.3 A crossing event is a case, and that is all it is.** Its query
program tests both ends before either update — the ordinary shape of a
case, which chooses what to ask and in what order. Its reply is

```
down the tail:   (F + id)* ∘ f
on the edge:     (L + id)* ∘ l
```

when the factorisation `g = l ∘ f` exists, and an arbitrary node-sort
morphism when it does not. Re-saturation is fused into the reply because
the reply is a morphism and morphisms compose; nothing is left unsaturated
behind an event. The factored/unfactored distinction survives only as a
**cost** regime (§11), never as a case distinction in the semantics.

**6.4 The hierarchy is automatic.** The two child saturations are this
same procedure one level in, bottoming out where a theory closes its own
local term. No annotation, no user-supplied locality, no declared
decomposition.

**6.5 Why this is not round-based iteration.** Iterating `X ↦ d + h(X)` by
rounds rebuilds a fresh object each round: every round misses the memo and
the cost is rounds × events × |X| in freshly built nodes. In 6.2 the
fixpoints sit *inside* the operator terms, so memoisation keys on
**saturated nodes**: a node is saturated once — *conditionally on the
certificate store honouring that memo's lifetime*, which is §10's
business and is now stated rather than assumed.

---

## 7. Queries

A query is what a case poses. It is the only construct that inspects data,
and it does nothing with what it finds — interpretation belongs to the
case that posed it.

**Definition 7.1 (`split_equiv`).** For a criterion `E` and data `d` at
sort `V`,

```
split_equiv(V, d, E)  →  a finite map  { residual ↦ code }
```

partitioning `d` by the residual `E` leaves after this subtree has been
consumed. Codes are subcodes of `d`; residual codes are canonical and
interned; classes that agree are merged before any client sees them; empty
classes are absent. **The domain of the reply is the discovered
alphabet.**

**7.2 The label is the residual, and `0` is adjoined to the codomain.**

- **One notion.** A query returns the criterion **curried by what this
  subtree consumed**. When nothing remains to consume, the residual is
  ground, and a ground residual is a value. Grouping classes by residual
  code and keying the memo on that code are the same act.
- **`0` is adjoined to the criterion's codomain.** A criterion is free to
  map part of its domain there and is not obliged to. That class is not
  represented, and computation stops on it. A filter junks; an `ite` does
  not. There is no boolean exception.
- **Criteria are total.** Partiality as a function — out-of-range indices,
  division — is a typing question settled at declaration (§12), not a
  runtime junking. `0` has exactly one meaning: this class is empty.

**7.3 The traversal, in three movements.** **Down**: travel to the first
position the criterion mentions; a criterion mentioning nothing in a
subtree returns there in one class without descending — locality is the
distance-zero case and it is free. **Across**: meeting a coordinate
grounds a subterm and renormalises, so the travelling criterion stays
ground in what it has consumed; normalisation must apply *while a residual
curries*, since currying is when residuals get their chance to coincide.
**Up**: results federate, §7.6.

**Definition 7.4 (grounding order, and termination).** Each query
**grounds at least one subterm** of the criterion, innermost first. Since
criteria are finite terms, a query program terminates. The measure is the
*term*, not the partition and not the frontier:

- **A class need not shrink.** If the queried coordinate is constant on
  the class, the reply has one class equal to the input — and the program
  has still progressed, because it grounded a subterm.
- **A position may be re-queried; a subterm may not.** Resolving
  `tab[tab[x]]` grounds `x` to `ℓ₁`, then `tab[ℓ₁]` to `ℓ₂`, then
  addresses `tab[ℓ₂]`: two queries land in the same leaf, at different
  addresses.

Where a case constructs criteria adaptively, termination falls to the
contract of §8.6.

**Definition 7.5 (kernel and labelling).** A partition is stored as its
**kernel** — the canonical, interned tuple of pieces — plus a
**labelling** from residuals into it. Splitting the two is what lets
partitions federate: agreeing on the kernel is a pointer test; disagreeing
on the labelling costs a finite table.

**7.6 The merge.** Two subqueries whose residual codes differ but whose
realised partitions agree are recognised on return and share one kernel,
keeping only their relabellings apart. Every partition federates,
including those that did not descend and those with a single class. This
is where weak normalisation is repaid retroactively (discipline 5), and
where the **equivariant collapse** appears — not as a code path, but as
what the merge finds when the structure is there.

**Theorem 7.7 (canonicity of the quotient).** Empty classes are never
represented; classes with equal parts are merged before any client sees
them; hence the discovered alphabet is the **coarsest refinement
compatible with `E` on `d`** — minimal, canonical, forced.

**7.8 Cost, structurally.** Per cut crossed between the criterion's
support and the queried cut, the traffic is

```
(# distinct realised normalised residuals — after merge: distinct kernels + tables)
  × distance (levels crossed)
  × class-code size in the theory
```

The first factor is a property of the criterion on the realised object;
the second is the shape's; the third is the theory's. §8.5 adds a fourth —
the price of relating two queries at a cut. No a-priori bound can match a
per-query bill, since a-priori bounds cannot see the data.

---
**Ex2 (residue) — the curried walk.** Shape `(⟨b⟩,(J₁,(J₂,⟨c⟩)))`, `b, c`
over a finite interval, junk sorts unconstrained, criterion
`(b+c) mod 3`. At `⟨b⟩` the residuals normalise to exactly three codes —
three subqueries regardless of the range of `b`; they travel the junk
ground; on return the merge finds one shared kernel (the cosets mod 3)
with three relabellings. The classes at `⟨b⟩` are the congruence classes
mod 3 — not intervals, not elements; whether they have cheap codes is the
theory's commitment, and the third cost factor isolated.

---

## 8. Cases

**Definition 8.1 (case).** A **case** at a node of sort `(V_h,V_t)` is a
function from the node's rectangles to morphisms at that sort,

```
case : ( ⟨r_e, P⟩ , ⟨r_t, S⟩ )  ⟼  a morphism at (V_h, V_t)
```

extended to data by linearity. It **owns its criteria**: it decides what
to ask, on which side, in which order, and may adapt what it asks next to
what it has learned. It is the only construct that opens a query and the
only one that closes one. The returned morphism depends on the classes and
not on anything else about the codes that realised them — which is what
makes the memo key `(r_e, r_t)` sound.

**8.2 Linearity is the only law.**

```
case(x ∪ y) = case(x) ∪ case(y)          case(0) = 0
```

Four consequences: the case is determined by its action on rectangles
(Theorem 3.1 supplies exactly that decomposition); the value does not
depend on which decomposition was used — §0's "elements are never
consulted", discharged rather than assumed; memoisation is sound; and a
case is an ordinary morphism of the hom-sets the calculus already has,
presented by a query program — not a new kind of thing.

**Proposition 8.3 (split decorrelates, case re-tensors).** The reply may
always be taken to be a finite sum of tensors `Σ_{k≤n} h_k^e ⊗ h_k^t`: the
correlation the criterion expressed has been consumed by the split. A case
remains free to return any node-sort morphism, including another
`query ; case` — how refinement and indirection are expressed with no
additional construct. The rank `n` at one class pair:

| `n` | reading |
|---|---|
| `0` | the pair contributes nothing — the empty sum, exact |
| `1` | deterministic relink |
| `> 1` | genuine choice at this class pair |

Nondeterminism lives in the **rank of the value**, never in the matching:
clause selection is deterministic, first match commits — sound because
matching is per-class, not per-word: a residual names a whole class, so no
word ever sees two clauses.

**8.4 Filters and assigns across a cut, defined.**

*Crossing guard* `a < b + c`, `a` in the head, `b, c` in the tail:

```
query the edge on a                  → residuals { (ℓ < b+c) ↦ P_ℓ }
per ℓ: query the tail on (ℓ < b+c)   → the positive class S_ℓ, or 0
reply: id ⊗ id  where S_ℓ ≠ 0;  the empty sum otherwise
```

*Crossing assign* `x ≔ y + z`, `x` in the head:

```
query the tail on (y+z)              → residuals { v ↦ S_v }
reply: assign(x ≔ v) ⊗ id
```

The edge parts may overlap rather than partition; Construction 3.3
absorbs it.

*Indirect assign* `tab[i] ≔ y + z`:

```
query on i          → { ℓ_i ↦ … }
query on (y+z)      → { ℓ_v ↦ … }
reply at (ℓ_i, ℓ_v): assign(tab[ℓ_i] ≔ ℓ_v)
```

This is the case that genuinely reaches the pair product, and the reason a
case must be a *function* and not a table: only the pairs that actually
meet are ever built.

**8.5 The dispositions, and the fourth cost factor.** A case relating two
sides may obtain what it needs several ways, differing only in cost:

| disposition | shape | cost |
|---|---|---|
| split the edge, specialise the tail | one head query, one tail application per class | `\|L_e\|` |
| split the tail, specialise the edge | the mirror; edge parts may overlap | `\|L_t\|` |
| split both, match by residual | only agreeing pairs survive | `\|L\|`, the diagonal |
| split both, relate freely | every meeting pair | up to `\|L_e\| · \|L_t\|` |

These are Reading 3.2's two congruence directions seen from the operator
side. The pairing reads as a join over discovered keys: matching by
residual is a hash join, driving from one side a nested loop, relating
freely a cross join — and choosing among them is join selection under
cardinality estimates the queries already computed. §16(v).

**8.6 The contract, with instruments.** A case is opaque by design — it
interprets residual types the engine has never heard of — so its
obligations are assume/guarantee, stated on the author. The author
guarantees:

1. **Linearity** (§8.2). Violated, the engine sums a decomposition into a
   wrong answer with no symptom.
2. **Class-determinacy** — the reply depends on the classes only.
   Violated, the memo returns a stale morphism.
3. **A well-founded query program** — §7.4's grounding order where
   criteria are fixed terms; some declared well-founded order where they
   are constructed adaptively.

*New in this draft:* each obligation has a **finite witness** — a pair
`(x, y)` breaking linearity; one class pair with two distinct replies; a
descent exceeding a declared bound — and the calculus requires the
**referee**: an instrument that, on a sampled fraction of a run, either
certifies the checked instances or produces the witness, into the invoice
(§12.5). The calculus does not guarantee the contract; it guarantees that
**violations are attributable**, at a sampling cost the runner chooses —
discipline 5 applied to trust. The same statement governs termination: the
phenomenon ledger names the two unboundedness sources (`∗`, and a case's
query program), and non-termination is attributable to one of them.

**8.7 A case is a presentation format, not a semantic construct.**
Extensionally, `query ; case` adds nothing: every morphism at a node is
expressible as a case — split by the tautological criterion, read the
answer off per class. The construct is the **intensional presentation
format** for morphisms at a cut, and the whole content is *which
presentations are cheap*. The spectrum's two ends: the **tautological
case** — maximal splitting, always correct, always expensive, the instance
that reproduces the structural decomposition itself; and the **trivial
case** — no split, the operation factors, `case` never called (§11).
Everything real sits between, and the modelling art is finding the
coarsest case that still expresses the operation.

**8.8 Labels are scoped to a case.** A residual exists between a query and
the case that posed it, and nowhere else. Outside a case there are only
morphisms and data. Consequently the calculus *structurally* never
interprets a residual, and both ends of a criterion are inside one case,
so the interchange theory is asserted by the case author, at one place.

**8.9 Label-based synchronisation.** Read §8.4 as a synchronisation
discipline: the parts of a shape are arbitrary, mutually ignorant
theories, and a crossing operation is a rendezvous conducted entirely in
residuals. One side offers a residual; the other answers the query indexed
by it; the case relinks. Neither side needs the other's algebra — only the
shared interchange sort, in the degenerate case only equality. That is
what lets sorts be **mixed freely**; and the alphabet of the rendezvous is
**discovered per query** — minimal, canonical, forced by Theorem 7.7 —
rather than chosen by a modeller. Systems whose every event factors never
invoke it; systems that need genuine correlation pay for exactly the
correlation they use.

---

## 9. Extension versus intension

**Theorem 9.1 (local constancy).** The graph of a total map is a finite
union of rectangles iff the map has finite image. **Corollary:** the
diagonal of an infinite carrier has no finite rectangle form — the
extensional encoding cannot express *no-op*, while the calculus has `id`
free twice (empty composition; a self-assign). Over finite carriers every
relation of out-degree `d` is a sum of `d` guarded assigns, so the
calculus subsumes extensional encodings, strictly at infinite theories.
Guards, by contrast, *are* data: selectors reduce to data, complete
morphisms do not.

**Definition 9.2 (trace).** The trace of `h` on explored `X` is
`graph(h) ∩ (X × ⟦V⟧)`. On any finite fragment the restriction has finite
image, so the trace is rectangle-representable *precisely and only
per-run*. The extensional encoding is what one gets by paying for that
certificate before the run. The same statement one level up: a case is
intensional, and its extensional table over residual pairs is exactly its
trace — accumulated as the memo of pairs that actually met, never built in
advance. Intension is primary; extension is the per-run certificate.

**The unit table.** One principle, four appearances — *never demand that a
unit be a citizen; let it be the empty operation of the relevant monoid*:

| monoid | unit | status |
|---|---|---|
| `(+,0)` on operations | `0` | adjoined — answer, never letter |
| pairing on data | `⊤` | never demanded — non-unital heads, the sink unmaterialised |
| `(∘,id)` on operations | `id` | free — the empty composition, the content of discipline 3 |
| rectangles across a cut | diagonal | unavailable over infinite carriers — the missing unit of the block ideal |

The extensional world is the world that insists on representing its units
as objects, and it is exactly over finite or tier-B structure — where
units happen to be representable — that the two worlds coincide.

---

## 10. Certificates and durability

*New as a section. Draft 3 established that memoisation is the runtime
construction of finite-index certificates (now §9.2) and leaned on it for
the saturation cost claim (§6.5) — while saying nothing about how long a
certificate lives. This section states the missing contract. It is a
contract about cost; by discipline 5 it can never be one about meaning.*

**Definition 10.1 (certificate).** A **certificate** is a memoised finite
fact about codes: a computed trace entry (`h` on this code gives that
code), a query's kernel and labelling, a node's saturation under a term.
Certificates are per-run, revocable, and cited by the codes they mention.

**10.2 The durability contract.** Every quantitative claim that reads
"once, ever" — a node is saturated once (§6.5), a kernel is federated once
(§7.6), a class pair meets a case once (§8.1) — is a claim **conditional
on the store**: it holds while the certificate lives and degrades to
recomputation when it does not. The store's retention policy is therefore
a *declared component of the cost model*, priced by the same invoice its
decisions affect (§12.5): retention and billing read the same meters.

**10.3 Attributable invalidation.** The one soundness rule: a certificate
may be dropped freely, and **must** be dropped no later than the release
of any code it cites — a code's name is never silently reused while
citations to its previous bearer stand. Freshness of naming is what makes
revocation checkable, and checkable revocation is what licenses aggressive
retention: a store that cannot attribute the death of a certificate must
kill them all at every collection, and the "once, ever" claims die with
them. (That degenerate policy is legal — discipline 5 — merely priced.)

**10.4 What survives a collection is a policy, not an accident.** The
interesting retention decisions are exactly the ones the invoice can see:
certificates whose cited codes survived; kernels with high federation
counts; saturation marks on nodes that recur. The calculus does not choose
a policy; it requires that the policy be *declared*, that its effects be
*billed*, and that its invalidations be *attributable*. Everything else is
engineering latitude.

---

## 11. Cuts, ranks, and the degeneration axes

Across a fixed cut, data has **section rank** — the number of primes,
forced by Theorem 3.1. Operations have **tensor rank** — the least `n`
with `h = Σ_{m≤n} ⊗_ℓ h_{m,ℓ}`. Tensors of local terms are local, so a
block-presented system stays in normal form; block operations along a cut
form a **non-unital ideal**, missing exactly the diagonal (Theorem 9.1).

**11.1 The two regimes for the interaction alphabet across a cut.**

- **Declared:** a finite letter set fixed a priori — an upper bound on
  tensor rank, constraining only the interface. Choice is externalised to
  the letters; within a letter, deterministic.
- **Discovered:** the per-query minimal alphabet of §7 — canonical,
  coarsest, priced by transport.

**The declared regime is exactly the fragment in which no query is ever
posed across a cut**: every event a tensor of local actions, §6 applying
with every reply of rank one and no split at all. Place/transition systems
live there permanently, under any decomposition — which is why they
saturate so well, and why a criterion travelling in such a system is a
symptom of bad encoding rather than expressive power.

**11.2 Value-blindness, replacing the convexity identification.** Draft 3
conjectured: declared regime = every crossing criterion convex, in the
theory-combination sense. **Refuted as stated**, by two stress cases:

- `a < b + c` across a cut lives in linear integer arithmetic, which is
  convex — yet its edge query yields one residual per realised value of
  `a` (coarsenable at best to threshold classes): a case is called, in a
  convex theory.
- `x = y` across a cut is pure equality — available in the *degenerate*
  interchange theory — and splits maximally: residual divergence as fast
  as the carrier.

So convexity of the theory neither prevents nor bounds case-calling. The
regime boundary is a property of the *criterion on the data*:

**Definition (value-blind).** A crossing criterion is **value-blind** on
`d` when its residual image under `split_equiv` is a singleton — the query
transports no information that varies with the data. **Declared regime =
every crossing criterion value-blind = every event factors.**

What survives of the conjecture: theory convexity plausibly governs the
**coarsening** a theory can perform on a residual family (collapsing
`ℓ < b+c` to threshold classes is precisely a convex-arithmetic argument)
— a bound on the *first cost factor* of §7.8, not on the regime. Stated as
a conjecture at that station. §16(iv).

**11.3 The degeneration axes.** The calculus degenerates along three
independent axes, and their origin is the classical flat setting:

| axis | general | degenerate | what degenerates |
|---|---|---|---|
| interaction | discovered alphabets, adaptive cases | value-blind, then absent | queries stop transporting, then stop existing |
| theory | structured codes, native fusion | enumerated: singleton codes, `split_equiv` by enumeration | the third cost factor goes nominal |
| shape | arbitrary tree, currified positions | the right comb: every head a leaf | positions collapse to a total order; cross-position sharing vanishes |

**The origin — all three axes degenerate — is the classical global-order
decision diagram calculus**, its schedule included: §6.2 at the origin is
the classical saturation, event for event.

Two requirements make the axes part of the calculus rather than an
implementation remark, and both are consequences of cost being the subject
of study:

- **Recognizability.** Each axis boundary is answerable — at declaration
  where it is structural (shape, theory tier), by an oracle where it is
  behavioural (value-blindness), under discipline 5 in either case. A
  degeneration that cannot be detected cannot be priced.
- **Free specialization.** The embedding of each degenerate calculus is
  full and faithful, and evaluation at a degenerate point costs what the
  degenerate calculus costs: **the general mechanism is never a tax on the
  special case.** This is an obligation (§16(iii)), not a slogan: it is
  what "the flat calculus is a value of this one" means, and it is
  falsifiable — a bill.

One invariant governs both ranks and the §7.8 bill: **residual
divergence** — how fast the residuals of realised criteria separate as the
head varies. On Ex1, every protocol-relevant criterion depends on the head
only through two boundary forks — at most four residuals at any size. On
Ex2, three at any range. When residuals separate as fast as the carrier —
two coordinates constrained equal across a cut — no mechanism helps, and
§15 says the one thing this document says about it.

---

## 12. The surface

The concrete syntax borrows SMT's: sorts and theories declared, terms
typed, new theories added by declaration rather than by extending the
grammar. Theory combination is what §8 does one level down, so surface and
semantics share a lineage.

**12.1 Declarations.**

```
(declare-leaf Fork (enum free tk))
(declare-leaf St   (enum T HL E))
(declare-leaf Tab  (array Int Int))
(declare-interchange LIA)

(declare-shape Ph  (pair (F Fork) (S St)))
(declare-shape V   (pair (pair Ph Ph) (pair Ph Ph)))
```

**12.2 Operations are two-phase.** Enabling and acting are the two things
a case separates, so the surface has exactly two slots:

```
(define-op takeL
  :when (and (= S T) (= F free))
  :do   (assign (S HL) (F tk)))
```

Whether this compiles to a local term or to a query/case bracket is
decided by where the mentioned positions sit, and the modeller is not told
which.

**12.3 Cases and discovered labels.** A case names its criterion and its
clauses; a default is **required**, because the alphabet is discovered and
the modeller cannot enumerate it:

```
(define-case shift
  :on   (mod (+ b c) 3)
  :when ((0 opA) (1 opB) (2 opC))
  :else junk)                        ; junk | id | an operation
```

Clauses are tried in order, first match commits (§8.3).

**12.4 The surface never mentions cuts.** A criterion is written over
positions; whether it crosses a cut, and which disposition of §8.5
resolves it, is the engine's. The choice is a cost decision only the
engine can price, and a shape is refactorable — a case written against a
cut would not survive re-shaping. Names are likewise the engine's problem:
symbols resolve to paths at declaration and re-index over the local shape
domain at use, so a theory receives currified names, never global ones.

**12.5 The invoice.** `(bill)` reports the cost model directly:

| counter | factor |
|---|---|
| distinct realised residuals per query | §7.8, first |
| levels crossed | §7.8, second |
| leaf calls and class-code sizes | §7.8, third |
| meeting pairs per cut | §8.5, fourth |
| certificates: held / dropped / recomputed, per policy | §10 |
| referee: instances checked, witnesses found | §8.6 |
| junked classes | diagnostic only — profiling, not algebra |

`(size X)` reports per-node prime counts, the §15 measure.

---

## 13. Theories, worked

**13.1 Enumerated.** An explicit finite carrier, coded by subsets. Tier B
trivially; `split_equiv` by enumeration. Its purpose is to isolate
calculus-level questions from theory-level ones — everything it does is
obviously correct, so a discrepancy is the calculus's — and to be the
baseline every other theory must beat.

**13.2 Boolean blocks.** A block of Boolean bits, coded by binary decision
diagrams over a **currified** variable set: one set of variables ever,
indexed from the terminal upward, shared by every position. All four tiers
free. Local terms are relational products — the theory fuses composition,
sum and star into one relation over interleaved current/next variables,
applied once. `split_equiv` has several query forms: by predicate (one
meet); by cube (realised valuations); by formula (distinct co-factors —
Theorem 3.1 one level down, performed by the theory).

**13.3 Bounded integer fields.** Finite domains over the same currified
Boolean variables: a field is a group of bits, `field = value` a code.
`split_equiv` by a field is the cube form; residuals are ground integers.
A bounded place/transition system is one field per place, transitions as
relational products, every event factoring: the origin of §11.3, no case
ever called.

**13.4 Linear integer arithmetic, and arrays.** The working interchange
theory and its companion. Criteria are LIA terms over coordinates in
different leaves; `split_equiv` by a LIA term returns classes indexed by
ground values; `tab[i]` is a select whose index is resolved by an outer
query and whose result may itself index another select. This pair is where
indirection lives, and where §7.4's term-grounding measure earns its keep.

**13.5 Recognisable sets.** The paradigm infinite theory: minimal
recognisers as canonical codes, tier B, concatenation and quotient as
exported maps. Every touched class finitely coded though the algebra is
infinite — discipline 4, exemplified.

**13.6 ω-recognisable sets (target).** Regular languages of infinite
words, coded by canonical deterministic recognisers where the fragment
admits them, with the residual language as the class notion. The natural
supplier of infinite-behaviour leaves — acceptance at the terminal stays
the default classification; what is infinite is the leaf's carrier, not
the shape. Declared here as a target import so the theory interface is
designed against it; not otherwise used in this document.

---

## 14. The streaming factorisation

Call a presentation a **finitary stream** if it is given by finite control
and finitely many registers valued in declared sorts, consuming the
frontier left to right, each step emitting local actions parameterised by
the letter read and the register contents, with admissible register
updates. Two load-bearing hypotheses: the stream's transition *tests* are
admissible criteria, and the configuration is *declared* — control finite,
every register sort an existing sort.

**Theorem 14.1 (factorisation; proof owed, §16(vi)).** Every
shape-respecting finitary stream satisfying both is extensionally equal to
a term of the calculus over local terms, cases, tensor-with-constants,
structural maps, `+` and `∘` — with, over finite shapes, **no `∗` at shape
level**: iteration survives only inside sequence-structured theories.
*Star-free over the shape; stars only in the alphabets.*

**Construction.** Thread the configuration as data; branch only on
control. Enlarge the sort by a configuration component, initialise by
tensor-with-constant, and let the stream become a straight-line
composition, one term per frontier leaf. Each per-letter step splits along
the phenomenon ledger: the control successor has finite image, hence is
blockwise by Theorem 9.1 — a case whose discovered alphabet it bounds; the
register and emission updates are possibly infinite-image but
single-valued — one assign per branch, inside the case's reply. Finally
marginalise the configuration away.

**Conjecture 14.2 (adjoined configuration).** A stream with genuine
cross-cut register flow is inexpressible over the original shape alone;
the *least* adjoined configuration sufficient for a behaviour is an
invariant of it — conjecturally the tensor rank of §11 in another gauge.

---

## 15. Remark on compression, at arm's length

The size of a representation is the sum over nodes of their prime counts,
and a node's prime count is fixed by Theorem 3.1: the number of distinct
realised sections at its cut. It is small exactly when the parts the shape
separates are mostly independent — when residual divergence is slow.
Choosing the shape that minimises total size contains variable ordering
for flat decision diagrams, so it is NP-hard, and some classifications are
large under *every* shape.

Currification adds an axis the flat setting does not have: sharing across
*positions*, not merely along the spine. A shape with many isomorphic
subtrees pays for its local structure once — why the philosophers'
representation is linear rather than merely sub-exponential — and this is
not available to any encoding in which each coordinate occupies its own
place in a global order. This is the claim a decision-diagram audience
will contest first; it deserves a theorem with Ex1 as witness family
(§16(vii)).

This document defines the calculus and prices its operations. Whether a
given semantics admits a shape and theory encoding under which it
compresses is the modeller's problem.

---

## 16. Obligations and seeds

In order of load-bearing-ness.

**(i) The transport lemma.** One lemma, two faces. *Mechanism face:* the
curried residual traversal of §7.3 computes the quotient — soundness of
currying, of code-level deduplication, and of the retroactive merge, with
the three-factor bill as the proved cost, and the induction on **term
grounding** rather than on frontier position. *Staging face:* per-node
continuation congruences, composed along the spine, reconstruct the full
congruence of the classified object. The bet is that these are one
induction read in two directions.

**(ii) The rewriting system.** §4.8 states the shape of the answer:
confluent rules over commutativity and relative support, canonical
representatives modulo the semiring laws, the merge laws, independence —
**with the oracle-stability requirement built into the statement**: rules
sound under every sound oracle, confluence up to extensional equality,
precision a bill parameter. Prove confluence, and prove the schedule of
§6.2 expressible as its rules. Expected hard, expected central.

**(iii) The degeneration theorems.** For each axis of §11.3: the embedding
of the degenerate calculus is full and faithful; the boundary is
recognizable (at declaration or by a sound oracle); and evaluation at the
origin coincides, operation for operation, with the classical flat
algorithm — free specialization as a theorem, not an aspiration. This is
the calculus's compatibility statement with its own tradition, and the
falsifiable form of "the general mechanism is never a tax".

**(iv) Value-blindness, characterised.** Per theory and criterion family:
when is a crossing criterion value-blind on all data (the declared
regime), and what coarsening of realised residual families does the theory
support (where the convexity of theory combination is conjectured to
re-enter, as a bound on the first cost factor). The two stress cases of
§11.2 are the test suite.

**(v) The direction rule.** §8.5 prices the dispositions but does not
choose. The choice is join selection under cardinality estimates the
queries already produce; import the machinery, or state a heuristic with a
regret bound. The last place where the calculus defers to a modeller
inside an operation — and §12.4 has promised the modeller it will not.

**(vi) The factorisation proof.** Complete Theorem 14.1 by the threading
construction, with cost transparency; then attack Conjecture 14.2.

**(vii) The sharing theorem.** State and prove §15's claim: a family of
classifications whose currified representation is linear while every
global-order representation is super-polynomial, with Ex1 as the witness
family.

**(viii) The generated category, stated once.** §4 defines operations as
terms with a code action and demotes the kernel to the extensional shadow.
State this with nothing informal left: which terms, which actions, which
quotient, and the closure argument that evaluability is preserved by `∘`,
`+`, `∗` — including the case construct, whose evaluability is contractual
(§8.6) and must be an assumption of the theorem rather than smuggled.

**Discharged since draft 3.** The convexity identification: refuted as
stated by the two stress cases of §11.2, replaced by value-blindness with
convexity demoted to a coarsening bound. Memoisation-as-remark: promoted
to the certificate contract of §10, which the "once, ever" claims now cite
instead of assuming.
