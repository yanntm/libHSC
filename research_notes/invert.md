# Inverting a term in HSC — the mathematics, the libDDD scheme, the mapping

Discussion note before the inverse is built (milestone M3 of
`ctl_directions.md`). Sources: libDDD `SHom.cpp` / `Hom.cpp` (`invert`,
`DomExtract`, `Compose::invert`, `Fixpoint::invert`), libITS
`ITSModel::getPredRel` (the exactness test and per-transition protection),
libITS `gal/ExprHom.cpp` (`_InvertExpr`: the symbolic inverse of an
assignment), and `include/hsc/core/algorithm.md` §9 as first written.

## 1. What an inverse *is* here, and why it costs the math nothing

Every operation term denotes an **additive** map `h : ℘(W) → ℘(W)`
(`h(A ∪ B) = h(A) ∪ h(B)`, `h(∅) = ∅` — Definition 2.2 of the paper). An
additive map on a powerset *is* a relation: `h(A) = {w' : ∃ w ∈ A, w R_h w'}`.
So every term already denotes a relation, and its inverse is the converse
relation, whose image map `pre_h(B) = {w : ∃ w' ∈ B, w R_h w'}` is additive
again. The inverse therefore **exists for every term** in the semantics the
paper already has. Nothing is added to the calculus's meaning; the only
questions are representability and finiteness:

* *representability*: does `pre_h` have a term in our algebra? For the
  structural fragment, yes, by the laws of converse below. At a leaf it is
  the theory's business, and one local action is missing (§4).
* *finiteness*: `pre` of a non-injective action is infinite on an unbounded
  coordinate (`x := 3` has every value as a predecessor). This is where a
  **context** enters: we only ever need `pre_h(B) ∩ D` for a finite `D ⊇ R`.
  `B ↦ pre_h(B) ∩ D` is additive too (intersection with a constant preserves
  unions), so it is again a relation: `R_h⁻¹ ∩ (D × W)`.

This is also why the inverse needs no complement and no top: `pre_h(B) ∩ D`
is built from meets and relative differences on data present, exactly like
everything else in the calculus.

**The one thing the paper says we do not have** is a preimage *in the theory
contract* (Definition 2.3). That stays true: the inverse is not a new
primitive owed by every theory; it is a capability a theory *may* offer,
and the calculus composes it structurally. See §4 for the contract.

## 2. The laws of converse, and locality

Writing `h⁻¹_P` for the inverse of `h` restricted to potential `P`:

    id⁻¹            = id
    (Σ hᵢ)⁻¹_P      = Σ hᵢ⁻¹_P                          union of relations
    node(h, t)⁻¹_P  = node(h⁻¹_{P_h}, t⁻¹_{P_t})         product of relations, P_h = ⋃ primes(P), P_t = ⋃ subs(P)
    (a ∘ b)⁻¹_P     = b⁻¹_P ∘ a⁻¹_{b(P)}                 reversed composition, re-potentialised
    lfp(h)⁻¹_P      = lfp(h⁻¹_P)                          (id ∪ R)* converse is (id ∪ R⁻¹)*
    sel⁻¹_P         = sel                                 a selector is a partial identity: self-converse

Two remarks carry the weight.

**Locality is preserved.** `node(h, t)⁻¹ = node(h⁻¹, t⁻¹)` and `id⁻¹ = id`:
an inverted term touches exactly the positions of the original, so the F/L/G
partition of the backward system at every cut is that of the forward one.
Backward saturation is `saturate()` over the inverted events, nothing more —
a proposition worth stating in the paper, since libDDD needs a
`dynamic_cast` per hom class to reconstruct it and we get it from `op_kind`.

**The composition rule is where libDDD is cunning.** The intermediate states
of `a ∘ b` live in `b(P)`, not in `P`: a multi-clause event
`(do …) (do …)`, or a crossing guard composed before a product, has
intermediate words that are not states at all. Inverting `a` against `P`
would restrict it to the wrong set (and could lose predecessors when `b(P)`
leaves `P`, or keep junk when `b(P)` is smaller). `Compose::invert` computes
`newpot = right(pot)` and inverts the left factor against it. In HSC this is
one `apply` of the forward factor to the potential diagram, exact, and it
also covers the guard case for free: `(p ∘ f)⁻¹_P = f ∘ p⁻¹_{f(P)}` — the
product is inverted against the *filtered* potential, tighter than `P`.

## 3. The potential: why a product, and what "intersection at top" means

libDDD's `DomExtract` fuses the arc values of a variable over the whole
potential and hands the leaf hom that set: the potential a leaf sees is the
**product of projections** `Π_x dom_x(P) ⊇ P`. Two HSC readings make the
choice precise rather than a heuristic:

* A product domain `Π_x D_x` *is a product selector* `Π_x keep(D_x)`, so
  `h⁻¹ ∘ keep_D` (equivalently, each leaf inverse intersected with its
  `D_x`) is a term with **the same locality as `h`**. The product of
  projections is the *largest* constraint inversion can carry without a
  straddling term — the largest that keeps saturation intact.
* The reachable set itself is not a product. Read as a term it is the sum of
  its rectangles, each a product filter straddling the root cut: `meet(·, R)`
  is a G term at the root, which is exactly "intersection at top level" and
  exactly what breaks saturation. So it is applied only where the product
  approximation is insufficient.

In HSC the projections need no separate extractor: inverting `node(h, t)`
against a potential diagram `P` hands `⋃ primes(P)` to the head (a join in
the head algebra — a diagram join when the head is composite:
internalisation) and `⋃ subs(P)` to the tail, and the recursion does the
rest. Memoising on `(term, potential)` keeps the sharing of isomorphic
positions whenever their projections coincide.

**Exactness and protection**, as `ITSModel::getPredRel` does it. Per event
`e`: `e⁻¹(R) ∖ R = ∅` says the product-restricted inverse never leaves `R`
on `R`, and the event is used raw. Otherwise the event is *protected*:
`meet(·, R)` after its application. libITS builds
`Σ_exact e⁻¹ + (Σ_protected e⁻¹) * reach` and saturates the sum; the
protected part is a G term chained at the top. Two ways to spell it here:

1. a **constant selector term** `within(D)`, `X ↦ X ∩ D` for a diagram `D`
   — additive, self-inverse, `within(D) ∘ within(D') = within(D ∩ D')`, the
   term reading of a diagram; `compose(within(R), e⁻¹)` is then an ordinary
   (straddling) summand the saturation rewrite classifies as G and chains;
2. or the checker chains the protected inverses by hand around the saturated
   closure of the exact ones.

Option 1 is one `op_kind` and keeps the schedule where it lives
(`saturate()`); it is also the piece the constrained-closure rewrite of
`ctl_directions.md` §4.4 will want later. Preferred.

Why is exactness ever violated on a P/T net? The inverse of a transition is
the transition with its arcs reversed, which preserves every P-invariant
(`Cᵀy = 0 ⇔ (−C)ᵀy = 0`), so `e⁻¹(R)` stays inside the invariant polytope —
but `R` may be a strict subset of the polytope (unreachable markings that
satisfy the invariants). Those are the spurious predecessors the test
catches. On `ring4` and the counter of the examples the inverse is exact.

**Soundness on `R` without protection.** A reachable state never has an
unreachable *successor*, so an unreachable predecessor found by an exact
inverse can only sit *behind* reachable states, never in front of one: a
backward closure from `B ⊆ R` is right on its reachable part with or without
`∩ R`, and negation is `R ∖ X` anyway. Protection is about the
*over-approximate* inverse (spurious edges, not merely unreachable states),
and about the size of the sets carried.

## 4. What the leaf contract gains — and what it does not

The support contract stays join / meet / minus + `apply_local` + `term_sum`
+ `term_lfp`. One virtual is added:

    invert_local(term, domain) → term       the converse of a local term, restricted to `domain`

with a default that **refuses** (a theory need not offer it; the calculus
reports `(unsupported)`, never approximates). `domain` is a code of the
theory — the leaf's projection of the potential. For `int_set`, whose local
terms are `guard g, then action`, the table (`D` the domain, `g∩D` an
extensional set computed once at inversion):

| forward | converse restricted to `D` | remark |
|---|---|---|
| `keep(g)` | `keep(g)` | self-converse |
| `shift(g, d)` | `shift(S, −d)` with `S = (g ∩ D) + d` | extensional guard: `u` is a predecessor iff `u − d ∈ g ∩ D`; H2 discharged by `D` |
| `assign(g, c)` | `keep({c})` then `choose(g ∩ D)` | the one **new action**: `x := any value of a set` |
| `apply(g, e)` | `Σ_{u ∈ e(g∩D)} keep({u}) then choose({v ∈ g∩D : e(v) = u})` | enumeration-honest; GAL's `invertExprValues` |
| `havoc(g, [lo,hi))` | `keep([lo,hi))` then `choose(g ∩ D)` | |
| `sum`, `lfp` | pointwise | |

`choose(S)` generalises the range havoc the theory already has (`havoc_if`
takes an interval; a set code is the honest general form). Every inverse
is restricted to `D` by construction — the product approximation of §3 is
applied uniformly at the leaves — so a closure over inverses terminates on
finite domains: this is the first place the calculus needs a domain for
*correctness* rather than for counting, and the declared `(leaf NAME LO HI)`
bound, when present, is the exact domain to prefer over the projection.

The math is untouched: `choose(S)` denotes the relation `g × S`, additive;
the theory still exports only pushforwards through `apply_local`; the
converse is spelled in the theory's own term language. Selector-hood
(paper §4.7) gains one fact: selectors are self-converse.

## 5. Case brackets: refuse now, invert symbolically later

A crossing term `when g do x_i := e_i` denotes
`{(w, w[x_i ↦ e_i(w)]) : g(w)}`. Its converse: given `w'`, the `w` that
agree with `w'` off the assigned coordinates, satisfy `g`, and hit `w'_{x_i}
= e_i(w)`. GAL's `_InvertExpr` handles it per coordinate: a constant
right-hand side gives `keep({c}) then choose(dom)`; a right-hand side that
is a function of the target alone is inverted on the domain's values; an
expression of other variables is resolved by *querying* them (the currying
the case engine already does); an array with an unresolved index is
resolved first. In HSC the same currying machinery exists (`split_equiv`),
and a bracket whose right-hand sides read no assigned coordinate inverts to
one bracket `when (w'_{x_i} == e_i) ∧ g[x ↦ v] do x_i := choose(D_{x_i})`
with the equality read on the *post* state — expressible with a `choose`
action and a guard. The general case (a right-hand side reading an assigned
coordinate, `x := x + y`) is a bracket per tuple of `Π D_{x_i}`, or an
algebraic inverse when `e` is a bijection in `x` given the rest.

None of this is needed for the MCC corpus (every P/T transition is
separable, Cor. 6.3), so **phase 1 refuses brackets that assign** and
inverts brackets that only guard (selectors: self-converse — a DVE model with
crossing guards and separable actions inverts). The symbolic bracket inverse
is its own milestone, after the CTL campaign says whether DVE matters here.

## 6. Decisions

1. `invert(term, potential)` in core: structural on `op_kind` by the laws of
   §2, memoised on `(term, potential)`; potentials are diagrams, projected by
   `⋃ primes` / `⋃ subs` at each `node`; `compose` re-potentialises with one
   forward application; `expr` inverts iff it is a guard-only bracket;
   `saturate`, `gfp` refuse (the caller inverts the flat events).
2. `support_algebra::invert_local(term, domain)`, default refusing;
   `int_set` implements the table of §4 with a new `choose(S)` action.
3. `op_kind::within(D)`, the constant selector, for protection — and later
   for the constrained-closure rewrite.
4. The exactness test per event, libITS's rule: raw when `e⁻¹(R) ⊆ R`, else
   `compose(within(R), e⁻¹)`.
5. Surface: `(invert NAME EVTERM POTENTIAL)` declares a named event term —
   the inverse relative to a bound result — so `(apply …)`, `(reach … from …)`
   and `(gfp …)` compose with it unchanged; `ctl` builds its `pred_events`
   from the declared events against `R` lazily.
6. The declared leaf bound is the domain when present, else the projection.

What to measure once it runs: how often protection triggers per model (the
exactness table), the node counts of backward sets against `R`, and the two
`UNKNOWN` expectations of `examples/models/ctl_*.hsc` flipping.
