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
`double`: state spaces are exponential and the number is for reading, not for
deciding.
