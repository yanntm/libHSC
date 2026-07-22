# Hierarchical Shape Calculus

*Working draft 1. Standalone: every object is defined from nothing, and the
document owes nothing. It is written to be the seed of its own iteration — a
reader (human or otherwise) holding only this text should be able to continue
the work. Section 14 lists where to push.*

---

## 0. The stance

The object of study is the **classification of structured words**. A word is a
tuple shaped by a tree; a classification assigns to each word a value in a
coefficient algebra; the default classification — the semantics when nothing
else is said — is **acceptance**: the value a word earns by reaching the
terminal. An object "is" the equivalence structure its classification induces
on words; a representation is a **tower of quotients** of that structure, one
quotient per cut of the shape; operations are morphisms between classification
spaces; and canonicity, everywhere in this calculus, means **uniqueness of a
quotient with respect to a stated classifier**. The structural normal form is
the special case where the classifier is the tautological one (the
continuation); the general case — quotient against a foreign classifier — is
an operation of the calculus itself (§9).

**The ontology.** Three layers, strictly ordered by citizenship:

- **Codes** are the citizens: finite, canonical, hashable. Every object a
  computation touches is a code.
- **Classes** are what codes mean, *relative to a classified object and a
  cut*: a code at a leaf names an element of the leaf's support algebra; a
  prime at an internal node names an equivalence class of the object's
  congruence at that cut. Classes are ephemeral and per-object; codes are
  stable.
- **Elements** of the underlying carriers are the extensional shadow. They
  fix what equality of behaviors *means*; they are never consulted to
  *compute* anything. A word read at any level of the hierarchy, without
  following links, is a word over that level's classes; there is no level at
  which raw elements are read.

This is **not** setoid mathematics. In a setoid the equivalence is ambient,
fixed, and global, and every operation carries a once-and-for-all proof of
respect. Here the congruence is relative to the classified object and the
cut — it is *X's* continuation congruence, discovered, not the carrier's.
Operations are not required to respect it and mostly do not: a filter slicing
across a prime refines the partition, and that is normal life.
Well-definedness is not assumed; it is *restored*, by re-canonicalization
through Theorem 5.1 after every action.

Three disciplines govern everything, and are stated before any definition
because every definition obeys them:

1. **Typing by adjunction.** Two constants are never citizens. `0` is
   *adjoined*: carriers are extended by a free point, so `0` is a legal
   *answer* and never a legal *letter* — nothing canonical stores, traverses,
   or points to it. `id` is *free*: the empty composition, never represented.
   Extending pointed definitions by adjoined elements, instead of admitting
   the elements into the definitions, is the single move that eliminates the
   traditional degenerate cases.
2. **Vision by staging.** Every rule accesses a sort through one of two
   windows: the *partition window* (construct equivalence classes: meets,
   relative differences, emptiness) or the *naming window* (compare canonical
   codes: equality, zero test). Recognition is staged — each node of a
   representation materializes exactly one congruence, its own cut's, and
   defers all others downward. Limited vision is not a restriction; it is the
   statement that the other congruences are other nodes' business. The
   discipline recurs on queries themselves (§9): a classifier is deduplicated
   through the naming window before it travels, and merged through the
   partition window when its answers return.
3. **Finiteness per element.** No carrier, alphabet, or algebra is required
   finite. What is required is that each *represented class* admit finite
   canonical codes and that each *run* of an operation touch finitely many
   codes. Global guarantees are replaced by per-element and per-run
   certificates, discovered rather than declared. Divergence of fixpoints is
   thereby located outside the representation theory — it is a property of
   iterations, not of objects — and is not this document's problem.

**Lineage, stated plainly.** The structural normal form generalizes the
compressed, trimmed decision-diagram forms over variable trees; "primes" and
"subs" are used deliberately in that tradition's sense, with the shape tree
in the role of the vtree. The continuation congruence is the classical right
congruence of word classification. The elementary layer is guarded-command /
Kleene-algebra-with-tests territory. The delta this calculus claims: semiring
coefficients with a *self-maintaining* zero-freeness invariant (§6), infinite
leaf carriers behind finite interface windows (§2), the operator calculus as
the primary semantics rather than a layer over an extensional one (§4), and
quotienting against foreign classifiers as a first-class canonical operation
(§9).

---

## 1. Coefficients

**Definition 1.1.** A **coefficient semiring** is a commutative semiring
`R = (R,+,·,0,1)` with decidable equality. It is **zerosumfree** if
`a+b=0 ⟹ a=b=0` and **zero-divisor-free** if `a·b=0 ⟹ a=0 ∨ b=0`; it is
**positive** if both. Throughout, `R` is positive. The Boolean semiring `𝔹`
is the default instance; nothing below changes for any other.

**Proposition 1.2 (the erasure morphism).** For finite-image maps into `R`,
let **weight erasure** send `X` to its support indicator
`supp(X) = 1_{{w : X(w) ≠ 0}}`. Then, pointwise: `supp(X+Y) = supp X ∨ supp Y`
for all `X,Y` iff `R` is zerosumfree, and `supp(X·Y) = supp X ∧ supp Y` (and
`supp(p ⊗ s) = p × supp s` for rectangles) iff `R` is zero-divisor-free.
Packaged: **erasure `𝒮_R → 𝒮_𝔹` is a morphism of the algebra of values iff
`R` is positive.**

The two laws have one job each, and Proposition 1.2 is that job stated once:
positivity of the semiring makes the support of every composite syntactically
visible from the supports of its parts, with no semantic test. This is what
lets the zero-freeness invariant of §6 maintain itself. What fails without it
is delimited honestly in the remark after Proposition 6.3: not canonicity,
but self-maintenance.

---

## 2. Leaf algebras

A leaf imports an external domain through a finite window; the window, not
the domain, is what the calculus sees.

**Definition 2.1.** A **support algebra** on a carrier `U` is a family
`ℛ ⊆ 2^U` containing `∅`, equipped with canonical finite codes for its
elements. Cumulative interface tiers:

- **E**: codes; decidable equality; decidable emptiness.
- **J**: E + finite joins.
- **G**: J + finite meets and **relative** differences `A ∖ B` — a
  generalized Boolean algebra: relatively complemented, distributive, least
  element, **no top required**.
- **B**: G + top and complement.

**Definition 2.2.** The **value algebra** `𝒮_R(ℛ)`: finite-image maps
`U → R` whose nonzero level sets lie in `ℛ`, canonically presented as
`Σ_k c_k·1_{A_k}` with the `A_k` disjoint nonempty and the `c_k` distinct
nonzero. Uniqueness of this presentation is elementary; `𝒮_𝔹(ℛ) ≅ ℛ`.

**Definition 2.3 (admissible maps, in two grades).** A leaf may export maps
`f : U → U'` as raw material for §7's assigns. The export comes in grades
matching the coefficient semiring:

- **image-computable**: the pushforward `f_* : ℛ → ℛ'` on codes is
  computable. Sufficient over `𝔹`.
- **fiber-computable**: additionally, weighted pushforward is computable —
  multiplicities `|f⁻¹(u') ∩ A|` are finite and computable on supports, or
  the leaf exports the weighted pushforward directly. Required over general
  `R`, where a non-injective map must transport weights, not just supports.
- **relative-preimage-computable** (optional, per map): the *relative*
  pullback `(A, B) ↦ f⁻¹(A) ∩ B` is computable on codes, `B` a relativizing
  support. The absolute preimage is the `B = ⊤` case and is priced
  accordingly at tier B — *relative difference is to complement as relative
  preimage is to inverse image*: the same no-absolutes discipline, on the
  data face and the map face, both bridged only where a top exists. Never
  assumed; where declared, it buys the collapsed normal form of
  Theorem 7.2. Unrelativized preimages are refused on principle as well as
  price: assignment is memory-destructive, and an absolute preimage asks a
  question about the ambient carrier — it steps outside the object under
  study, out of the realized zone.

A leaf may also export admissible endo-operators used by §8.

**Definition 2.4 (normalization strength).** A leaf exports canonical codes
not only for its support elements but for the *closed terms of its operation
language* (the expressions that will travel as curried classifiers, §9). The
strength of this normalization — which equalities of terms it decides — is a
declared property of the leaf. It is a **cost parameter, never a soundness
parameter**: undetected equalities cause redundant subqueries, which the
semantic merge of §9 cleans up retroactively; they never cause wrong answers.

**Per-element finiteness, exemplified.** The carrier may be an infinite free
monoid and `ℛ` the recognizable subsets, coded by their minimal recognizers:
an infinite algebra, tier B, in which every *class* is finitely named —
equality and emptiness decided on codes, and each class having finitely many
residuals under continuation. This is the paradigm case of discipline 3: the
calculus never needs the algebra bounded, only the class coded.

---

## 3. Shapes

**Definition 3.1.** Fix a family of imports. **Shapes**:

```
V ::= 1 | ⟨A⟩ | (V_h , V_t)
```

with denotations `⟦1⟧ = {•}`, `⟦⟨A⟩⟧ = U_A`,
`⟦(V_h,V_t)⟧ = ⟦V_h⟧ × ⟦V_t⟧`. Shapes are the free magma on the imports with
a unit sort adjoined. A **word** of sort `V` is an element of `⟦V⟧`; the
**frontier** of `V` (leaves, left to right) exhibits words as tuples read in
order, bracketed statically by the tree.

Three structural commitments:

- **No names.** Positions are paths; ordered trees are rigid; two equal
  subterms are one object under hashing. Naming, where needed (§7), is a
  decoration consulted by exactly one construct.
- **No associativity.** `(V₁,(V₂,V₃))` and `((V₁,V₂),V₃)` are distinct sorts
  with a *computed* isomorphism between them (§7, structural maps). The
  quotient of shapes by associativity flattens every shape to a sequence of
  leaves; refusing that quotient is what "hierarchical" means here.
- **The unit sort is mono-sorted.** `1` exists at one shape only;
  classification values live there (`𝒮(1) = R`); it is the codomain of the
  default classifier — acceptance — and the domain of data (§4).

---
**Ex1 (philosophers) — statics.** Four philosophers in a ring. Fork `F_i`
sits between philosopher `i` and `i+1` (indices mod 4); philosopher `i` uses
`F_i` (left) and `F_{i−1}` (right). Leaves, all tier B:
`F_i ∈ {free, tk}`, `S_i ∈ {T, HL, E}` (thinking, holding-left, eating).
Shape, balanced: `V = ((Ph₁,Ph₂),(Ph₃,Ph₄))` with `Ph_i = (⟨F_i⟩, ⟨S_i⟩)`.
Everything below generalizes to `2^k` philosophers on the balanced shape of
depth `k+1`; the point of the example is that its representations stay linear
in the number of philosophers while its state space is exponential.

---

## 4. Behaviors and data

**Definition 4.1 (behaviors are terms with a code action).** For shapes
`V, W`, the hom-set `hom(V,W)` is **generated**: the elementary layer of §7
(`filter`, `assign`) and the structural maps, closed under composition `∘`,
finite sum `+`, and certified iteration `∗` (§8). A behavior **acts on
diagrams** (§6), i.e. on codes: each generator's action is a leaf-interface
operation — meets for `filter`, pushforward for `assign`, rearrangement for
structural maps — followed by re-canonicalization (Theorem 5.1); composite
behaviors act by composing actions. Hom-sets are pointed join-semilattices
(`+`, `0`); composition is bilinear and `0`-strict; `h` applied to data may
answer `0` — deadlock — and nothing is ever undefined.

Two consequences of taking terms-with-action as the definition:

- **Admissibility is a theorem, not a hypothesis.** The leaves certify the
  generators (`filter(e)` because `ℛ` has meets, `assign(π̄ ≔ f)` because `f`
  exports a computable pushforward of the right grade, Definition 2.3), and
  closure under `∘`, `+`, `∗` preserves evaluability by construction. No
  behavior is ever checked against a semantic admissibility condition.
- **No infinite sums are ever formed.** Applying a behavior to data is a
  code-level operation whatever the cardinality of the classes involved:
  pushforward on a code is one leaf operation whether the class it names has
  three elements or continuum many. Per-element finiteness reads: every class
  a run touches has a finite code, and the run touches finitely many codes.

**Definition 4.2 (the extensional shadow).** Where leaves code singletons, a
behavior determines a pointwise kernel `w ↦ h(w) ∈ 𝒮(W)₀`. **Extensional
equality** `h ≡ h'` is agreement of kernels on all words. The shadow is the
*specification* of equality — the court in which identities like Theorem 7.2
are judged — and never the *mechanism* of evaluation. The calculus is the
largest thing that computes; the extensional world exists only as the
standard its computations answer to. (§10 makes the asymmetry a theorem.)

**Definition 4.3 (data).** `hom(1,V) = 𝒮(V)₀`: an element of data — a state
set, a weighting — is a behavior of the point. Constants are arity-0
behaviors; they factor through `1` (forget, then emit).

**The two faces.** A support element `e ∈ ℛ(V)` types two ways:

- as a **point** `ê ∈ hom(1,V)` — "these words";
- as a **sub-identity** `filter(e) ∈ hom(V,V)` — "keep these words".

One canonical code, two typings. The sub-identities below `id` are exactly
the filters (§7), and `e ↦ filter(e)` carries `∩` to `∘` and `∪` to `+`: the
support algebra is the idempotent monoid of the category. The two faces
interconvert only where a top exists (`ê = filter(e) ∘ ⊤̂`): the bridge is
priced at tier B, and most of the calculus never buys it.

---

## 5. The quotient theorem

The single central statement; everything else is bookkeeping around it.

**Theorem 5.1 (canonical decomposition).** Let `X ∈ hom(1,(V_h,V_t))` be
nonzero. There is exactly one expression

```
X = Σ_{i=1..n} 1_{P_i} ⊗ S_i
```

with the `P_i ∈ ℛ(V_h)` nonempty pairwise disjoint **(D)** and the
`S_i ∈ 𝒮(V_t)` nonzero pairwise distinct **(F)**: the `S_i` are the distinct
nonzero **sections** `X(x,–)`, the `P_i` their fibers. Uniqueness is by
evaluation alone once left factors are constrained Boolean — and Boolean
left factors are themselves forced: the scaling slack
`(c·p) ⊗ s = p ⊗ (c·s)` of rank-one terms is rigidified by making one factor
qualitative. *Decisions are qualitative, values quantitative* — invisible
over `𝔹`, mandatory over every other coefficient semiring.

**Reading 5.2 (continuation congruence).** Define, for `X` and a cut:
`x ~ x'` iff `X(x,–) = X(x',–)` — prefixes indistinguishable under all
continuations. (This is the classical *right congruence* of word
classification, generalized to values.) Theorem 5.1 says: the primes are its
classes, constructed through the partition window; the subs are the class
*names*, compared through the naming window. Dually `X` induces a congruence
on the tail coordinate (the joint kernel of the sections); the node does not
materialize it — the subs' own decompositions do, level by level down the
spine, bottoming out in leaf codes. **A representation is the classification's
congruence tower, materialized one cut per node and deferred elsewhere.**
That is discipline 2 as a theorem-shape; its formal statement is obligation
(iii) of §14.

**Construction 5.3 (compilation, with its interface ledger).** From any
finite rectangle expression `X = Σ_j a_j ⊗ t_j`: form cells
`c_σ = (⋂_{j∈σ} a_j) ∖ (⋃_{j∉σ} a_j)` over nonempty sign patterns σ (meets
and *relative* differences: tier G, head), sections `S_σ = Σ_{j∈σ} t_j`
(joins: tier J, tail), merge equal sections (equality: tier E, tail), drop
zeros. The one cell whose expression would need a top — the all-negative
pattern — has section `0` and is deleted by the pointed discipline before it
is written: **non-unital heads and partial decompositions each fill exactly
the hole the other leaves.** Over positive `R`, sums of nonzero tails are
nonzero, so compilation of zero-free inputs never creates a zero section;
zero tests are only ever imported at leaves.

**The degeneracy ledger.** Canonical form outlaws the four ways of saying one
thing twice:

| rule | kills | licensed by | consumes |
|---|---|---|---|
| no zero subs | padding | `p ⊗ 0 = 0` (smash: definitional under pointing) | tail zero test |
| no zero primes | phantom classes | fibers of nonzero sections are nonempty | head emptiness (sound *and complete* — the one semantic decision procedure canonicity leans on) |
| (D) | overlap | — | head disjointness |
| (F) | refinement | distributivity `(p∪p')⊗s = p⊗s ∪ p'⊗s` | tail equality |

**Definition 5.4 (representable).** `X` is **representable over `V`** iff at
every cut of `V`, hereditarily, its continuation congruence has finite index
and its classes are expressible in the head's algebra. Over finite shapes
with admissible leaves this is automatic; over sequence-like leaves it is a
per-element certificate (discipline 3). Representability is the *only*
finiteness this calculus ever demands.

---

## 6. Diagrams

**Definition 6.1.** For each shape, **diagrams**: `Diag(1)` = canonical codes
of `R`; `Diag(⟨A⟩)` = canonical leaf codes; `Diag(V_h,V_t)` = the adjoined
`0`, or a finite nonempty set `{(p_i,s_i)}` with `p_i ∈ Diag_𝔹(V_h) ∖ {0}`
pairwise disjoint and `s_i ∈ Diag(V_t) ∖ {0}` pairwise distinct. Arcs draw
from the unpointed carrier; `0` enters only as an answer.

**Theorem 6.2 (representation).** Decoding is a bijection
`Diag(V) ≅ 𝒮(V)₀` onto the representable elements. Hashing makes equality a
pointer test and `0` an absence — the null reference is the one legitimately
sort-generic citizen precisely because it is not a citizen.

**Proposition 6.3 (self-maintenance).** Over positive `R`: sums of zero-free
diagrams are zero-free (zerosumfreeness), products and rectangles likewise
(zero-divisor-freeness), with no semantic test — supports are computed by
weight erasure (Proposition 1.2). The loop: smash makes `0` an absence;
absence makes emptiness at every composite sort a pointer test; free
emptiness makes zero-freeness free to maintain; maintained zero-freeness
synthesizes tier E at every composite sort from leaf imports alone. **The
invariant polices itself, and the boundary of the calculus is the boundary of
this loop.**

*Remark (the boundary, honestly).* Canonical decision-diagram forms over
cancellative coefficients exist and are legion — algebraic and multi-terminal
diagrams over rings, clock and zone diagrams, and more. What cancellation
breaks is not canonicity but the **erasure morphism** (Proposition 1.2):
`supp(X+Y)` may be strictly below `supp X ∨ supp Y`, so zero-detection stops
being weight erasure and becomes a recursive semantic obligation. Over finite
carriers that obligation is dischargeable bottom-up by hash-consing — which
is exactly why those diagram families work. Over infinite tier-E leaves it
may be undischargeable. The precise claim of this calculus is: *the
self-maintenance loop, and with it tier-E synthesis at composite sorts from
leaf imports alone, is available iff erasure is a morphism* — iff `R` is
positive.

**Theorem 6.4 (internalization; hierarchy as closure).** For any shape whose
head-position subtrees are hereditarily tier G, the Boolean diagrams over it
form an admissible support algebra (tier G; tier B if all leaves are B),
coded by their hashes. Hence `⟨Diag_𝔹(V)⟩` is a legal import: **the calculus
eats its own output, and "hierarchical" is this closure property**, not a
feature. The typing law it induces: head subtrees uniformly G, tail spines
pay-as-you-go — *classes need differences; names need a hash.*

**Reading 6.5 (the alphabet tower).** Along the frontier, a diagram is a trim
partial recognizer whose letters are *computed*: the letters at a node are
its primes, forced by Theorem 5.1; level k's alphabet is level (k+1)'s state
set. A leaf is the same pair *(recognizer, continuation)* with the recognizer
imported — a black box; an internal head is the recognizer computed — a
white box; internalization says a white box can be re-boxed black, which is
why opacity is harmless: every algebra's window bottoms out at the shared
point `0` and equality. The strictness options are recognizer hygiene:
*partial = trim* (dead ends are absent transitions), *total = completed*
(the sink's defining set is `⟦V_h⟧ ∖ ⋃P_i` — totality costs a complement,
tier B); *normalized = states strictly sorted by level*, *trimmed = collapse
of degenerate levels* (the unitor quotient). All four corners are canonical
where their prerequisites exist; none quotients associativity.

---

## 7. The elementary layer

**Definition 7.1.** The **elementary calculus** is generated under `∘` from:

- **`filter(e)`**, `e ∈ ℛ(V)`: the sub-identity at `e`. Filters only
  discard; they are exactly the behaviors `h ≤ id`, and the admissible ones
  are exactly the multipliers of §4.
- **`assign(π̄ ≔ f)`**: a **complete morphism** — total, single-valued —
  rewriting the coordinates at paths `π̄` by admissible maps of the current
  word. Assigns only move; they never discard. The vector form (**parallel
  assign**) is a genuine primitive: sequential assigns cannot express
  simultaneous exchange without temporaries. Assigns are the sole
  position-addressed construct — the one consumer of the naming decoration.

Alternation and iteration are deliberately **not** elementary (§8): their
exclusion is what makes the next theorem true.

**Theorem 7.2 (elementary normal form).** Elementary terms compose as
partial functions, so the elementary layer is **deterministic by theorem**.
Adjacent filters merge by meets (`filter(e) ∘ filter(e') = filter(e ∧ e')`,
tier G) and adjacent assigns merge by composition of admissible maps into
one parallel assign; hence every elementary term equals a **guarded word**

```
filter(e_k) ∘ assign(f_k) ∘ … ∘ assign(f_1) ∘ filter(e_0)
```

strictly alternating, with the free `id` filling empty slots — `id` is a
legal degenerate assign and the empty filter, so missing guards cost nothing
and no `⊤` is ever materialized (discipline 1, padding the word).
Pre-controls, moves, post-controls: **composed in place, never commuted.**
A guarded word is a quantifier-free sequence of (guard, update)
instructions — the form constraint engines consume natively; the image
computation of the relations-as-data alternative is an existential
projection, and this normal form never performs one.

Guards do not commute freely with assigns, in either direction, for two
different reasons:

- **Rightward is unsound.** `assign(f) ∘ filter(e) ≠ filter(f_*e) ∘
  assign(f)` for non-injective `f`: the image guard accepts collisions —
  words outside `e` whose image lands in `f_*e`. Assignment is
  memory-destructive; what it forgets, no post-guard can recover.
- **Leftward is licensed only relatively.** The shadow identity
  `filter(e) ∘ assign(f) = assign(f) ∘ filter(f⁻¹e)` uses the *absolute*
  preimage, which is doubly illegitimate: it is a tier-B object in
  disguise (the relative preimage taken against `⊤`), and it can be
  unrepresentable outright — on `ℕ` with interval supports and
  `f = (mod 2)`, `f⁻¹{0}` is the even numbers, no finite union of
  intervals, while the same preimage *relativized to the realized*
  `[0,9]` is `{0,2,4,6,8}`: representable, at a code size scaling with the
  support (the third cost factor of §9). What the interface may export
  (Definition 2.3) is the relative preimage, and the sound pull-through
  correspondingly **merges into the preceding guard**:

  ```
  filter(e) ∘ assign(f) ∘ filter(b) = assign(f) ∘ filter(f⁻¹e ∩ b)
  ```

  A guard pulls left *onto a guard*, never past one, and never into the
  free `id` — `id` is not `filter(⊤)` and provides no relativizer. When a
  term is applied, the support of the data acted on is the outermost
  relativizer: wp is always wp *relative to what is there*.

**The collapsed regime.** For terms over maps exporting the relative
preimage, every pull-through has the preceding guard — or, in application,
the data's own support — to land in, wp is substitution-then-meet, and the
guarded word collapses to `assign(F) ∘ filter(E)`: **one move, one guard**.
Finite tier-B leaves export even the absolute preimage for free; elsewhere
the relative form is the honest operation, and the collapse is available
exactly where it is exported. (Application alone never needs wp at all —
pushforward then meet suffices; the relative preimage earns its keep in
normalization, caching, and backward reasoning.)

**Theorem 7.3 (the guard algebra).** The sub-identity interval `[0, id_V]`
is a **unital Boolean algebra even over non-unital supports**: its top is
`id`, which is free, and complement is `(¬h)(w) = {w}` iff `h(w) = 0` —
computed on data as a *relative* difference, tier G, no `⊤` anywhere.
Consequences: (i) full propositional structure on guards — preserved by wp
where wp exists (the collapsed regime of 7.2), substitution passing through
`∧,∨,¬` unchanged; (ii)
`¬filter(e)` is an admissible *morphism* whose defining set need not be
admissible *data* — the morphism algebra strictly outruns the data algebra,
an instance of a pattern completed in §10.

**Proposition 7.4 (the operator semiring is half positive, the right half).**
The behavior semiring is **zerosumfree** (`h + h' = 0 ⟹ h = h' = 0`: no
cancellation, hence no subtraction, hence monotone fixpoints) but **not
zero-divisor-free** (`filter(e) ∘ filter(e') = 0` with both nonzero when
`e ∧ e' = 0`: disjoint guards are honest zero divisors). The coefficient
discipline recurs one level up with exactly the law that was load-bearing —
zerosumfreeness — and without the one whose job (syntactic supports of
products) the operator level never needed done. Negation, banished from the
semiring by zerosumfreeness, survives exactly and only on the idempotent
interval, where the free unit gives it something to be relative to.

**Structural maps** (between sorts, along maps of shapes): unitors
`𝒮(1,V) ≅ 𝒮(V) ≅ 𝒮(V,1)`; the computed associator; the swap of coordinates;
leaf isomorphisms; marginalization `Σ_h : 𝒮(V_h,V_t) → 𝒮(V_t)` (tail joins,
tier J); duplication `V ↦ (V,V)`. These are the sort-changing citizens; they
exist only along legitimate shape maps, which is the typing discipline doing
its job.

**The phenomenon ledger.** Each construct owns one phenomenon and provably
introduces no other:

| construct | layer | owns |
|---|---|---|
| `filter` | elementary | partiality (deadlock) |
| `assign` | elementary | data movement |
| `∘` | elementary | sequencing, and guard-conjunction (`M_e∘M_f = M_{e∧f}`) |
| `+` | derived | nondeterminism |
| `∗` | derived | unboundedness |
| `case` | derived (§9) | data-dependent control |
| semantic merge | derived (§9) | quantification over a support |

Deadlock provenance: `h(w) = 0` traces to a filter. Choice provenance:
`|h(w)| > 1` traces to a `+`. Branching provenance: control depending on the
word traces to a `case`. Quantifier provenance: any bounded universal traces
to Θ's merge — the elementary layer performs none, ever.

---
**Ex1 (philosophers) — protocol and decomposition.** The protocol, one
elementary term each, already in the collapsed form of 7.2 (finite tier-B
leaves export preimages for free):

- `takeL(i) = assign(S_i≔HL, F_i≔tk) ∘ filter(S_i=T ∧ F_i=free)`
- `takeR(i) = assign(S_i≔E, F_{i−1}≔tk) ∘ filter(S_i=HL ∧ F_{i−1}=free)`
- `drop(i)  = assign(S_i≔T, F_i≔free, F_{i−1}≔free) ∘ filter(S_i=E)`

The ledger, instantiated: partiality lives only in the filters; `drop` and
`takeR` are the parallel assign earning its primitive status (two coordinates
written simultaneously, no temporaries); and exactly two terms cross the top
cut — `takeR(1)` (touches `F_4`) and `takeR(3)` (touches `F_2`) — the
boundary forks, nothing else. The system is
`step = Σ_i (takeL(i)+takeR(i)+drop(i))`; the reachable set is
`X = step∗(x̂₀)` from all-thinking, all-free. The state space is finite, so
the run's residuals are finite and the `∗` certificate of §8 is discharged
trivially — discipline 3's degenerate happy case.

Invariant of `X`: `F_i = tk` iff `S_i ∈ {HL,E}` or `S_{i+1} = E`, never
both. Inside the head `(Ph₁,Ph₂)`, `F_1` is determined by head-local data;
`F_2` is not — it can be taken because `S_2 ∈ {HL,E}` (the head knows) *or*
because philosopher 3 is eating (the head cannot know). A stored code whose
meaning is half remote: this is the two-windows discipline in one bit.

Theorem 5.1 at the top cut, by hand: tail-compatibility depends on the head
through `e₁ = [S_1 = E]` and the fork-2 situation, which the invariant
resolves into `σ₃ ∈ {S_3=E, S_3≠E}`. All four combinations `(σ₃, e₁)` are
realized, so `X` has **exactly four primes** at the top cut — e.g.
`P_{(S_3=E),0}` = reachable heads with `S_2 ∉ {HL,E}`, `F_2 = tk`, `S_1 ≠ E`:
"the fork is taken and nobody on my side holds it." Classes, visibly not
elements. Each sub re-decomposes at the `(Ph₃,Ph₄)` cut by the same boundary
analysis one level down — Reading 5.2 executed: the tail congruence the top
node deferred is materialized by the subs' own decompositions. On the
generalized instance, every cut of the balanced shape is crossed by two
forks, so every node has at most four primes at any number of philosophers:
representation linear, state space exponential.

Construction 5.3, instantiated: compiling `X` from the rectangles that
`step∗` accumulates, the all-negative cell is precisely the unreachable
heads; its section is `0` and it is deleted before being written — no top
over the head algebra is ever formed. Deadlock: the all-`HL` word is
reachable (each `takeL` fires once); from it every guard meets the residual
emptily, and the `0`-answer's provenance traces to the `takeR` filters, per
the ledger.

---

## 8. The derived layer

`+` is the enrichment's join; `∗` its closure, `h∗ = Σ_n hⁿ`, defined when
the ambient fixpoint exists. Convergence is a **per-run certificate**:
`h∗(X)` converges iff the residuals the run explores stay finite —
discovered, not declared, per discipline 3. Any two fair iteration schedules
agree on the value and on existence, by monotonicity alone (Prop. 7.4 gives
monotonicity structurally); scheduling therefore lives strictly below the
semantics and is out of scope by construction, not by omission.

---

## 9. The quotient constructor

The calculus can decompose against classifiers other than its own
continuation. This is the operation that removes the last a-priori bound in
the framework.

**Definition 9.1.** Let `E : V ⇝ W` be elementary (the **classifier**), `W`
an existing sort. Define

```
Θ_E ≔ (decompose at the cut (W, V)) ∘ ⟨E, id⟩_*
```

— graph the classifier (one parallel assign into the pair sort `(W,V)`),
then apply Theorem 5.1 at the new cut. The result of `Θ_E(X)` is a canonical
family `{(Λ_a, X_a)}`: the **discovered alphabet** `Λ(X,E)` = the realized
value-classes of `E` on `X`, and per letter the part of `X` it classifies.

**Theorem 9.2 (canonicity of the quotient).** By the pieces alone:
(i) empty classes of `E` are never represented — zero-freeness at the
virtual node is zero-freeness anywhere; (ii) letters with equal parts are
merged *before* any client sees them — compression (F) at the new cut; hence
(iii) `Λ(X,E)` is the **coarsest refinement compatible with `E` on `X`**:
minimal, canonical, forced. Per-query alphabet discovery inherits the
uniqueness of Theorem 5.1 wholesale.

**Mechanism 9.3 (curried residual transport).** The definition is one global
decomposition; the computation is a traversal, governed by one invariant:

> **The traveling classifier is always ground and suffix-supported** — at
> each node it mentions only coordinates not yet consumed. Meeting a
> coordinate substitutes its class and renormalizes: the classifier
> *curries* as it travels.

At a node, upon receiving classifier `E` for object `X`:

1. **Partition the head-primes by normalized curried residual code.** For
   head-class `p`, the residual `E_p = E(p, –)` is normalized by the leaf's
   term normalization (Definition 2.4); classes with equal residual codes
   are grouped. Deduplication by code *is* cache sharing: every context
   whose classifier curries to the same code hits the same entry, and a
   consumed coordinate can never be re-queried, because the residual no
   longer mentions it.
2. **Recurse, one subquery per distinct residual code**, on the tails
   actually demanded.
3. **Semantic merge on return.** Returned partitions with equal underlying
   kernels are merged, recording the finite relabeling tables between their
   letter sets. This step is where normalization weakness is repaid: two
   residuals whose codes differ but whose realized partitions agree merge
   here, retroactively.
4. **Assemble** by (F)-compression at the cut, yielding the canonical
   `{(Λ_a, X_a)}` of Definition 9.1.

**Reading 9.4 (two windows on queries).** Discipline 2 recurs on classifiers
themselves: syntactic deduplication before sending is the naming window
applied to queries; semantic merge after return is the partition window
applied to their answers. Queries are citizens, and they get quotiented like
everything else.

**The equivariant harvest.** When the merge of step 3 finds that all
residuals share a single kernel up to relabeling — as when `E` factors
through a finite operation with head-independent residual kernel — the run
memoizes into **one subquery plus a finite table**. This is not a declared
mode; it is what the general mechanism collapses to when the structure is
there. Discovered, not declared, again.

**Definition 9.5 (case).** For per-letter elementary branches `k`:

```
case_E(k) ≔ μ ∘ (Σ_{a ∈ Λ(X,E)} k(a) ⊗ 1_a) ∘ Θ_E
```

— quotient, act per class (each branch invoked **once per congruence
class**, by 9.2(ii)), re-fuse. Extensionally
`case_E(k) = Σ_a k(a) ∘ filter(E⁻¹a)` — linear, inside the operator
semimodule; `E⁻¹a` here is shadow notation, the fibers being the parts `Θ`
discovers by forward decomposition, never a leaf preimage — but the
intensional content is the point: the branch set is
discovered, minimal, and canonical, not declared. `case` owns data-dependent
control in the phenomenon ledger; note the tautological instance —
`E` = the continuation itself, codomain existing by internalization
(Theorem 6.4) — reproduces the structural decomposition: **the calculus
classifies against its own values, and its normal form is the special case
of its own quotient operator.**

**Cost, structurally (transport).** Per cut crossed between the classifier's
support and the queried cut, the traffic is:

```
(# distinct realized normalized residuals)   — after merge: distinct
                                               kernels, plus finite tables
  × distance (levels crossed)
  × class-code size in ℛ of the discovered classes
```

Three factors. The first is a property of the classifier on the realized
object — how fast residuals diverge as the head varies (§11). The second is
the shape's business; locality is the distance-zero case — a classifier
supported in one subtree is free outside it. The third is the leaf's
business: which congruence classes the support algebra can *name cheaply* is
a modeling commitment of the leaf choice, exactly as which cuts communicate
cheaply is a modeling commitment of the shape choice. Those are the two axes
of modeling in this calculus. No a-priori bound can match a per-query bill,
since a-priori bounds cannot see `X`.

---
**Ex1 (philosophers) — a quotient query.** Classifier
`#eaters = Σ_i [S_i = E] : V ⇝ ⟨ℕ⟩` — elementary, codomain a *declared
infinite* sort. `Θ_{#eaters}(X)` discovers `Λ = {0, 1, 2}`: not `ℕ`, not
`{0..4}`, but exactly the realized classes (two eaters maximum, and only
non-adjacent, in a ring of four). Empty classes never represented; letters
with equal parts merged before any client sees them; a three-letter answer
extracted from an infinite codomain. Declared regimes bound; discovered
regimes bill.

---
**Ex2 (residue) — the curried walk.** Shape `V = (⟨b⟩,(J₁,(J₂,⟨c⟩)))` with
`b, c` ranging over `[0,9]` in `X`, the junk sorts `J₁, J₂` unconstrained.
Classifier `E = (b+c) mod 3 : V ⇝ ⟨ℤ/3⟩`.

At `⟨b⟩`, the residuals `(v+c) mod 3` normalize (Definition 2.4: the
arithmetic leaf rewrites `(3+c) mod 3` to `c mod 3` at query construction)
to exactly three codes: `c mod 3`, `(1+c) mod 3`, `(2+c) mod 3` — three
subqueries **regardless of the range of `b`**. They travel the junk curried
and ground; the junk contributes nothing and re-queries nothing. On return,
the semantic merge finds one shared kernel — the cosets mod 3 — with three
relabeling tables: the equivariant harvest, one subquery memoized, one finite
table kept.

The classes at `⟨b⟩` are `P₀ = {0,3,6,9}`, `P₁ = {1,4,7}`, `P₂ = {2,5,8}`:
**not intervals, and not elements** — Theorem 5.1 forces these classes to
exist, and whether they have cheap codes is the leaf's commitment. Over
finite-unions-of-intervals, `P₀` costs a number of intervals linear in the
range; over an ultimately-periodic code family, `P₀` is one constant-size
code at any range. Same tower, same three letters, different bill — the
third cost factor of §9, isolated. For contrast, unrestricted `a = b+c` on
the same `X` discovers nineteen letters, range-dependent and still finite
per-run; and `a = b+c+d` curries in stages — at `b`, the residual
`(v+c+d) mod 3` is again one of three codes, and every context currying to
the same code shares one cache entry.

---

## 10. Extension versus intension

When is a behavior *data*? Encode extensionally over the doubled shape
`(V,V)`; compare.

**Theorem 10.1 (local constancy).** The graph of a total map is a finite
union of rectangles iff the map has finite image. **Corollary:** the diagonal
of an infinite carrier has no finite rectangle form — the extensional
encoding cannot express *no-op*, while the calculus has `id` free twice
(empty composition; `assign(π ≔ π)`). Over finite carriers every relation of
out-degree `d` is a sum of `d` guarded assigns, so the calculus subsumes
extensional encodings, strictly at infinite leaves: rectangles encode only
finite-image (blockwise) behavior across a cut; `assign` imports genuinely
infinite-image maps through the leaf interface. Filters, by contrast, *are*
data (§4's two faces): the asymmetry of the elementary basis — selectors
reduce to data, complete morphisms do not — is this theorem speaking.

**Definition 10.2 (trace).** The **trace** of `h` on explored `X` is
`graph(h) ∩ (X × ⟦V⟧)`. On any finite fragment the restriction has finite
image, so the trace is rectangle-representable *precisely and only per-run*:
memoization is the runtime construction of the finite-index certificate —
per-element finiteness, for operators. The extensional encoding is what one
gets by paying for that certificate before the run.

**The unit table.** One principle, four appearances — *never demand that a
unit be a citizen; let it be the empty operation of the relevant monoid*:

| monoid | unit | status |
|---|---|---|
| `(+,0)` on homs | `0` | adjoined — answer, never letter |
| pairing on data | `⊤` | never demanded — non-unital heads, partial decompositions, the sink unmaterialized |
| `(∘,id)` on behaviors | `id` | free — the empty composition |
| rectangles across a cut | diagonal | unavailable over infinite carriers — the missing unit of the block ideal (§11) |

The extensional world is the world that insists on representing its units as
objects, and it is exactly over finite or tier-B structure — where units
happen to be representable — that the two worlds coincide.

---

## 11. Cuts and the two ranks

Across a fixed cut, data has **section rank** — the number of primes,
discovered, forced by Theorem 5.1. Behavior has **tensor rank** — the least
`n` with `h = Σ_{m≤n} ⊗_ℓ h_{m,ℓ}`, sums of tensors of local behaviors.
Tensors of elementary terms are elementary (guards conjoin under `∘`;
updates merge into one parallel assign), so a block-presented system stays
in normal form with one guarded word per letter — one (guard, move) pair in
the collapsed regime; and block behaviors
along a cut form a **non-unital ideal** of the operator semiring, missing
exactly the diagonal (Theorem 10.1).

Two regimes for the interaction alphabet across a cut:

- **Declared:** a finite letter set fixed a priori — an upper bound on
  tensor rank, constraining only the interface (inside a component, branches
  are arbitrary elementary). Choice is externalized to the letters; within a
  letter, deterministic.
- **Discovered:** the per-query minimal alphabet `Λ(X,E)` of §9 — canonical,
  coarsest, priced by transport.

The declared regime is exactly the fragment in which `Θ` is never invoked;
the discovered regime removes the bound at the stated cost. The parallel is
precise: *declared letter sets are to discovered alphabets as declared leaf
partitions are to discovered primes* — upper bounds a modeler accepts in
exchange for never transporting.

One invariant governs both ranks and the §9 bill: **residual divergence** —
how fast the residuals of realized classifiers separate as the head varies.
Example: on Ex1, every protocol-relevant classifier's residual depends on the
head only through the two boundary forks — at most four residuals, ever, at
any size. On Ex2, residuals separate only up to residue — three, at any
range. When residuals separate as fast as the carrier — two coordinates
constrained equal across a cut is the canonical case — no mechanism helps,
and §13 says the one thing this document says about it.

---

## 12. The streaming factorization

Call a behavior presentation a **finitary stream** if it is given by finite
control `Q` and finitely many registers valued in declared sorts, consuming
the frontier word left to right, each step emitting elementary actions
parameterized by the letter read and the register contents, with admissible
register updates. (This is the natural machine model for one-pass evaluation
over the frontier — and it is the *only* presentation style available to a
formalism whose sole cut is front-versus-rest.) Two hypotheses, both
load-bearing:

- **(H1)** The stream's transition *tests* are admissible predicates — they
  live in the leaves' support algebras. (Updates being admissible maps is
  not enough; branching needs its own clause.)
- **(H2)** The configuration is *declared*: control is finite and every
  register sort is an existing sort of the shape's import family.

**Theorem 12.1 (factorization; proof owed, §14(iv)).** Every shape-respecting
finitary stream satisfying H1–H2 is extensionally equal to a term of the
calculus over: leaf-internal elementary maps, filters, parallel assigns,
tensor-with-constants, structural maps, `case`, `+`, `∘` — with, over finite
shapes, **no `∗` at shape level**: iteration survives only inside
sequence-structured leaves, absorbed into leaf operators. *Star-free over
the shape; stars only in the alphabets.*

**Construction.** Thread the configuration as data; branch only on control.
Enlarge the sort to `(W_conf, V)` with `W_conf = ⟨Q⟩ × R₁ × … × R_m` (finite
`Q` is a trivially declared tier-B leaf; the `R_j` are declared by H2).
Initialize by tensor-with-constant `(q₀, r̄₀)`. The stream becomes a
straight-line composition, one term per frontier leaf, associated to the
shape tree by structural maps. Each per-letter step splits along the
phenomenon ledger: the control successor has finite image (at most
`|Q| ×` test outcomes), hence is blockwise by Theorem 10.1 — a `case` whose
discovered alphabet is bounded by it; the register and emission updates are
possibly infinite-image but single-valued — **one parallel assign per
branch**, the letter read, the registers written, and the emissions applied
simultaneously, no temporaries. Finally marginalize the configuration
component away (structural map, tier J; for nondeterministic streams the
tail-join is exactly the sum over runs that `+` means). Finite shape means
finitely many steps, hence no `∗` at shape level; sequence leaves absorb
their own iteration internally. The factorization is cost-transparent by
construction: per step, at most `|Q| ×` tests branches — the machine model
is a compilation target that never bought anything the calculus cannot
state.

The candidate counterexamples to the *hypotheses* are streams whose
configuration lives in no declared sort. Under H2 they are excluded by
fiat — unbounded occurrence counting over `⟨ℕ⟩` with successor is just
`assign(r ≔ r+1)`, declared and harmless — so the honest residual question
is not whether the theorem holds but what the enlargement costs:

**Conjecture 12.2 (adjoined configuration).** A stream with genuine
cross-cut register flow is inexpressible as a term over `V` alone: the
detour through the enlarged sort `(W_conf, V)` is unavoidable, and the
*least* adjoined configuration sort sufficient for a given behavior is an
invariant of that behavior — conjecturally the tensor rank of §11 across the
relevant cuts, in another gauge.

---

## 13. Remark on compression, at arm's length

The size of a representation is the sum over nodes of their prime counts,
and a node's prime count is fixed by Theorem 5.1: the number of distinct
realized sections at its cut. It is small exactly when the parts the shape
separates are mostly independent — when residual divergence (§11) is slow —
and Ex1 shows that natural systems with local communication achieve it.
Choosing the shape that minimizes total size is the hierarchical analogue of
variable ordering for flat decision diagrams: it contains that problem
(comb-shaped trees), so it is NP-hard, and some classifications are large
under *every* shape — two coordinates constrained equal over a large
carrier force as many primes as carrier values at any cut separating them.

This document defines the calculus and prices its operations. Whether a
given semantics admits a shape and leaf encoding under which it compresses
is the modeler's problem, and this document deliberately says nothing more
about it.

---

## 14. Obligations and seeds

For the next iteration, in order of load-bearing-ness:

**(i) The generated category, stated once.** §4 defines behaviors as terms
with a code action and demotes the kernel to the extensional shadow. State
this as a definition with nothing informal left: which terms, which actions,
which quotient (extensional equality), and the closure argument that
evaluability is preserved by `∘`, `+`, certified `∗`.

**(ii) The tensor, universally.** `hom(1,(V_h,V_t))` as a tensor of pointed
modules with Theorem 5.1 as its canonical-basis theorem, making the
Boolean-primes gauge functorial rather than legislated.

**(iii) The transport lemma.** One lemma, two faces. *Mechanism face:* for
elementary `E`, the curried residual traversal of Mechanism 9.3 computes
`Θ_E(X)` — soundness of currying, of code-level deduplication, and of the
retroactive semantic merge, with the three-factor bill of §9 as the proved
cost. *Staging face:* per-node continuation congruences, composed along the
spine, reconstruct the full congruence of the classified object — the formal
content of "a representation is the congruence tower" (Reading 5.2). The bet
is that these are one induction read in two directions.

**(iv) The factorization proof.** Complete Theorem 12.1 by the threading
construction, with cost transparency; then attack Conjecture 12.2 — the
least-adjoined-configuration invariant and its relation to tensor rank.

**(v) Presentation normal forms.** Theorem 7.2 canonicalizes one elementary
term to a guarded word. Canonical representatives for *sums* of guarded
words modulo the semiring laws, the merge laws, and the collapsed-regime
wp-commutations — the behavior-layer analogue of Theorem 5.1 — and, in the
same stroke, the normalization theory of
classifier codes whose strength Definition 2.4 declares and §9 prices.
Expected hard, expected central.

**(vi) The weighted pass.** Carry Ex1 and Ex2 through §§5–9 over `ℕ`
weights — on Ex2, `Θ_{(b+c) mod 3}` counts representations, and the
fiber-computable grade of Definition 2.3 stops being optional — to confirm
the qualitative/quantitative split (Boolean primes, weighted subs) never
needs patching.
