# `core/` — algorithms

Abstract description of the calculus as implemented. Self-contained: every
definition the code relies on is stated here, in the code's own vocabulary.

## 1. Codes

Everything a computation touches is a **code**: a 32-bit id into an intern
table. Code `0` is **absence** — the pointed discipline's adjoined empty
element, present in every algebra. It is never stored, never traversed,
never an arc. Because it is absence:

* emptiness is `c == 0` — free, no virtual call, no theory involved;
* equality is `a == b` — free, hash-consing has already done the work.

So the support-algebra contract reduces to exactly **three**
operations that anyone must actually implement: join, meet, relative
difference. Equality and emptiness, also owed, are discharged by interning.

## 2. Shapes

    shape ::= unit | leaf(theory) | pair(head, tail)

Interned, so `(V₁,(V₂,V₃))` and `((V₁,V₂),V₃)` are different codes: no
associativity, which is what "hierarchical" means. No names — a position is
a path, and two equal subterms are one object under hashing.

## 3. The support algebra, and why it is type-erased

A head position's primes live in *some* algebra: a leaf theory's, or — when
the head is itself composite — the diagram algebra. This is
**internalisation**: the diagrams of a sort themselves satisfy the support
contract, so a composite sort imports exactly like a leaf. One node's
canonization must call meet and minus on its primes without knowing which of
those it is. That is a runtime question: the shape says, and shapes are data.

So `support_algebra` is an abstract class with three virtuals, and the
registry maps a shape code to the algebra of that sort. The cost is one
indirect call per meet — against a meet that is either a vector intersection
or a recursive diagram descent, both orders of magnitude larger.

Diagrams implement the same interface as any leaf theory. That is
internalisation made structural rather than remembered: there is no "leaf
case" and "node case".

## 4. The normal form (the canonical decomposition)

A nonzero diagram at sort `(V_h, V_t)` is a finite nonempty set of arcs

    { (P_i, S_i) }   with   P_i ∈ ℛ(V_h),  S_i ∈ Diag(V_t)

subject to the degeneracy ledger:

| rule | kills | how it is enforced here |
|---|---|---|
| no zero subs | padding | an arc with `sub == 0` is never written |
| no zero primes | phantom classes | an arc with `prime == 0` is never written |
| **(D)** primes pairwise disjoint | overlap | the canonicalizer (§5) maintains it |
| **(F)** subs pairwise distinct | refinement | the final regroup by sub |

Stored as a node: `sort`, `arity`, and a trailing array of `(prime, sub)`
pairs **sorted by prime code**. The sort is in the node because codes are
position-relative: the same arc list under two different sorts means
two different things, and the primes would be codes in two different algebras.
Sorting the arcs is what makes the interned representation canonical, so
equality of diagrams is equality of ids.

`0` is absence, and the terminal is the unique node of sort `unit` with no
arcs. Hence emptiness and the terminal test are both integer comparisons.

## 5. The one canonicalizer

Everything that builds a diagram funnels through this. Input: any finite bag
of rectangles `(P, S)`, overlapping and repeating freely. Output: the unique
normal form.

    canonize(rectangles):
      cells = []                       # arcs, primes pairwise disjoint
      for (P, S) in rectangles:
        if P == 0 or S == 0: continue          # smash, before anything
        rest = P
        next = []
        for (Q, T) in cells:
          if rest == 0: next += (Q, T) ; continue
          M = meet(Q, rest)
          if M == 0:   next += (Q, T) ; continue
          Qrest = minus(Q, M)
          if Qrest != 0: next += (Qrest, T)    # Q's part that P misses
          next += (M, union_tail(T, S))        # the overlap: sections add
          rest = minus(rest, M)
        if rest != 0: next += (rest, S)        # P's part no cell covered
        cells = next
      regroup cells by sub: join the primes of every group   # (F)
      drop any cell whose sub is 0                           # smash again
      sort by prime ; intern

The sieve — the inner loop over the cells — is what an application pays
for at every node whose head is touched. It is skipped when the primes are
known to be pairwise disjoint already: when the head term is `id`, and when
the head's algebra answers `injective(term)` — a guard, a shift, anything
that is an injective partial function on elements keeps disjoint primes
disjoint. Only the regroup by sub (F) remains. The answer is advisory and
`false` is always safe; a theory that never answers just pays the sieve.

Two things to see in it.

**The overlap takes the union of the sections.** A bag of rectangles denotes
their *sum*, so where two heads overlap the tails add. That union is a
diagram union at the tail sort, which is itself a canonization — the mutual
recursion that makes this one algorithm rather than two.

**The all-negative cell never appears.** The cell whose expression would
need a top has section `0`, and the pointed discipline deletes it before it
is written. Concretely, `rest` is carried by *relative* difference only; no
complement is ever formed, so the three-map contract suffices and no theory
is ever asked for a top.

Every prime written is a nonempty meet or a nonempty relative difference of
primes already present — which is the invariant that keeps (D).

## 6. The set algebra, over the same construction

At sort `unit`: `join(1,1) = 1`, `meet(1,1) = 1`, `minus(1,1) = 0`. Absence
handles every other case.

At a composite sort, all three reduce to a bag of rectangles and one
canonization:

    join(a, b)   = canonize( arcs(a) ++ arcs(b) )

    meet(a, b)   = canonize{ (M, meet_tail(S,T))
                           | (P,S) in a, (Q,T) in b, M = meet_head(P,Q) != 0 }

    minus(a, b)  = canonize( { (P \ ⋃_j Q_j, S)          | (P,S) in a }
                           ∪ { (P ∩ Q_j, S \ T_j)        | (P,S) in a,
                                                           (Q_j,T_j) in b } )

The head operations recurse into the head's algebra (a leaf theory, or the
diagram algebra again); the tail operations recurse into the tail sort. Zero
results are dropped by canonize itself, so none of these needs a special
case for emptiness.

This is the SDED union re-expressed: the legacy algorithm is the same
incremental insert, written for one sort instead of any.

## 7. Counting

`cardinal(diagram)` = `Σ_i |P_i| · cardinal(S_i)`, with `|P_i|` asked of the
head algebra and `cardinal(unit) = 1`. Memoized like everything else. It is a
`double` by default: state spaces are exponential and the number is for
reading, not for deciding. The same recursion instantiated on a GMP integer
(`cardinal_exact`, its own memo) gives the number itself when a query asks
for it (the MCC state count).

## 8. The deflationary closure (`gfp`)

`lfp h` accumulates: `X ↦ X ∪ h(X)` from a seed upward. Its dual removes:

    gfp(h)·A  =  the greatest fixpoint of  X ↦ X ∩ h(X)  below A

by iteration from `A` downward, stabilising because `A` is finite and every
round is a subset of the previous. Offered as a term (`op_kind::gfp`, one
operand) for the same reason `lfp` is: a closure recognised at the term
level is one memo entry, not a round count. There is no meet on terms
(`h ∩ id` is not a term); the closure is the only deflationary form the
calculus spells.

What it computes: for `h = next`, the states of `A` that have a predecessor
in `A`, iterated — the states of `A` reached from a cycle inside `A` (the
forward SCC hull `EH`); for `h = pred`, the states of `A` from which an
infinite path stays in `A` (the core of `EG`). Evaluation is breadth-first
at the sort the term is applied at, over the memoised `apply` of the
operand: no F-L-G schedule is known for it (a cycle may alternate parts),
and none is attempted here.

## 9. The inverse of a term, relative to a context

Every term denotes an additive map, hence a relation; its inverse is the
converse relation, restricted to a finite **potential** `P ⊇ R` so that a
non-injective action has finitely many predecessors. Nothing is added to the
meaning of terms; `pre_h(B) ∩ P` is additive again. The construction is
structural (`research_notes/invert.md` has the discussion):

    id⁻¹            = id
    (Σ hᵢ)⁻¹_P      = Σ hᵢ⁻¹_P
    node(h, t)⁻¹_P  = node(h⁻¹_{⋃ primes(P)}, t⁻¹_{⋃ subs(P)})
    (a ∘ b)⁻¹_P     = b⁻¹_P ∘ a⁻¹_{b(P)}        the left factor sees the image of the right
    lfp(h)⁻¹_P      = lfp(h⁻¹_P)
    selector⁻¹      = selector                  a partial identity is self-converse

An inverted term touches exactly the positions of the original, so the
saturation partition of the backward system is that of the forward one:
backward saturation is `saturate()` over inverted events. `saturate` and
`gfp` terms are not inverted (the caller inverts the flat events); a case
bracket inverts iff it only guards (it is then a selector).

**The potential is projected, not carried.** At `node(h, t)` the head is
inverted against the join of the primes and the tail against the join of the
subs: each leaf ends up restricted to its projection of `P` — the product of
projections, which is the largest constraint expressible as a product
selector, hence the largest inversion can carry without a straddling term.
The declared domain of a leaf replaces the projection when present.
Memoised on `(term, potential)`, so isomorphic positions with equal
projections share their inverse.

**At a leaf** the theory inverts its own local terms:
`invert_local(term, domain)`, an optional capability of the support contract
whose default refuses. For `int_set` (`guard g` then action; `D` the domain,
`g ∩ D` an extensional set):

| forward | converse restricted to `D` |
|---|---|
| `keep(g)` | `keep(g)` |
| `shift(g, d)` | `shift(S, −d)`, `S = (g ∩ D) + d` |
| `assign(g, c)` | `keep({c})` then `choose(g ∩ D)` |
| `apply(g, e)` | `Σ_{u ∈ e(g∩D)} keep({u})` then `choose({v ∈ g ∩ D : e(v) = u})` |
| `havoc(g, [lo,hi))` | `keep([lo,hi))` then `choose(g ∩ D)` |
| `sum`, `lfp` | pointwise |

`choose(S)` — `x := any value of the set S` — is the one new action; the
range havoc is its interval case. Every inverse is restricted to `D`, so a
closure over inverses terminates on finite domains (hazard H2 discharged by
the domain).

**Exactness and protection.** With the product restriction an inverted event
may produce states outside `R` — spurious predecessors, since `R` is not a
product. Per event: `e⁻¹(R) ∖ R = ∅` and the event is used raw; otherwise it
is composed with the constant selector `within(R)` (`X ↦ X ∩ R`, an
`op_kind` of its own: additive, self-inverse, the term reading of a
diagram), a straddling summand the saturation rewrite chains as G. On the
reachable part of a backward closure the raw inverse is already right — no
reachable state has an unreachable successor — so protection is about
spurious edges and about the size of the sets carried, and is applied only
to the events that need it.

## 10. The existential image (`has_image`)

A second reading of the same terms. `has_image(h, S)` answers the question
"is `h(S)` empty?" by producing a **witness**: a nonempty `W ⊆ h(S)`, or `0`
exactly when `h(S) = 0`. It never builds `h(S)` when a part of it will do.
Correct by the additivity of every term (`h(A ∪ B) = h(A) ∪ h(B)`): a
witness of a part is a witness of the whole.

    has_image(id, S)          = S
    has_image(Σ hᵢ, S)        = the first has_image(hᵢ, S) ≠ 0, else 0
    has_image(a ∘ b, S)       = W_b := has_image(b, S); 0 if W_b = 0;
                                 else W_a := has_image(a, W_b); W_a if ≠ 0;
                                 else has_image(a, b(S))                 (a may miss the witness of b)
    has_image(node(h, t), d)  = the first arc (P, T) of d with
                                 w_h := has_image(h, P) ≠ 0 and w_t := has_image(t, T) ≠ 0,
                                 as the rectangle (w_h, w_t); else 0
    has_image(lfp h, S)       = S                                       (a closure contains its seed)
    has_image(saturate…, S)   = S
    has_image(within(D), S)   = S ∩ D
    has_image(expr, S)        = expr(S)                                  (the case engine, in full)
    at a leaf                 = the theory's has_image_local, by default apply_local

The interesting case is the **deflationary closure**. `gfp(h)·S ≠ 0` iff `S`
holds a cycle of `h`; a cycle found with a *part* of `h` inside a *part* of
`S` is a cycle of the whole:

    has_image(gfp h, d) at a composite sort, h = Σ hᵢ flattened,
      F = { t : node(id, t) ∈ h },  L = { g : node(g, id) ∈ h }:
        for each arc (P, T) of d:
          w := has_image(gfp(Σ F), T)   ≠ 0  →  rectangle(P, w)        a cycle below the cut, head fixed
          w := has_image(gfp(Σ L), P)   ≠ 0  →  rectangle(w, T)        a cycle in the head, tail fixed
        else gfp(h)·d in full

Soundness: a set `X ⊆ T` with `X ⊆ F(X)` (a post-fixpoint of `X ↦ X ∩ F(X)`)
gives `P ⊗ X ⊆ h(P ⊗ X)` since `node(id, t)` leaves the head alone, so
`P ⊗ X ⊆ gfp(h)·d`. The recursion reaches the leaves, where a cycle within one
coordinate is decided by the plain iteration `X ↦ X ∩ h(X)` in the leaf
algebra (no leaf term is needed for it). Completeness is the fallback's: a
cycle that alternates parts is only found in full. This is libDDD's "fast
SCC detection" (`Fixpoint::has_image`), stated for terms.

The per-arc cycle witness is **opt-in** (`diagram_engine::set_fast_cycle_witness`):
where no component cycles on its own the descent — sub-hulls for every arc
at every level — is paid and the full hull follows anyway; on the MCC
corpus that loses more than it wins, so by default `has_image(gfp h, d)`
is the hull itself.

`has_image` is memoised like `apply` (the traversal order is fixed, so the
witness is a function of its arguments). What it buys: a `nonempty?` question
on a filtered set stops at the first arc that passes; whether a set holds a
cycle is answered without the cycle hull when a component cycles on its own.

## 11. Composition of product terms

A term composed with a selector — `t ∘ sel` (filter, then step: the
constrained forward closure) or `sel ∘ t` (step, then filter: the backward
one) — is what a fixpoint under a constraint iterates. Written as a
`compose` term it is a straddler: it names no side, so the saturation split
puts it in G and the closure degrades to breadth-first. But relations on a
product compose **componentwise**:

    node(a, b) ∘ node(c, d)  =  node(a ∘ c, b ∘ d)          hence node(a, id) ∘ node(id, d) = node(a, d)
    (Σ aᵢ) ∘ b = Σ (aᵢ ∘ b)         a ∘ (Σ bⱼ) = Σ (a ∘ bⱼ)
    id ∘ b = b,   a ∘ id = a

so the composition of two product terms is a product term, whose support is
the union of the two, and the saturation split classifies it like any
event. `compose_at(sort, after, before)` applies these laws down the shape
and hands the leaves to the theory (`term_compose`, an optional capability
whose default refuses); when a leaf refuses, the composition stays a
`compose` term at that node — correct, merely not fused. Nothing else
composes structurally (`lfp`, `gfp`, `saturate`, `within`, a case bracket).

**At a leaf**, `int_set` composes `(g₂, act₂) ∘ (g₁, act₁)` — apply the
first, then the second — into one primitive when it can: the guard is
`g₁ ∧ g₂[after act₁]` and the action `act₂ ∘ act₁`:

| `act₁` | `g₂` after it | `act₂ ∘ act₁` |
|---|---|---|
| `keep` | `g₂` | `act₂` |
| `shift d` | `g₂[x ↦ x + d]` (an extensional `g₂` shifted by `−d`) | `keep → shift d`, `shift e → shift (d+e)`, `assign c → assign c`, `choose S → choose S`, `apply e → apply e[x ↦ x + d]` |
| `assign c` | the constant `g₂(c)`: `false` is the zero term | `keep → assign c`, `shift e → assign (c+e)`, `assign c' → assign c'`, `choose S → choose S`, `apply e → assign e(c)` |
| `choose S` / `havoc` | `S' := S ∩ g₂` (empty: zero) | `keep → choose S'`, `shift e → choose (S'+e)`, `assign c → assign c`, `choose T → choose T`, `apply e → choose e(S')` |
| `apply e` | `g₂[x ↦ e]`, symbolic `g₂` only | `keep → apply e`, `shift d → apply (e + d)`, `assign c → assign c`, `choose S → choose S`, `apply e' → apply e'[x ↦ e]` (no modulo) |

Guards conjoin as their kinds allow: two symbolic guards by `conj`, two
sets by `meet`, a set against a symbolic guard by `filter`. Sums compose
pointwise. Anything else (a closure, a modulo transform under substitution)
refuses, and the composition stays unfused at the node above.
