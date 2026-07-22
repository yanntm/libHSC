# hsc/core — algorithms

Shapes, diagrams, the canonical decomposition, the support algebra. This
package knows nothing about classifiers or operations.

---

## 1. Objects

A **shape** is `V ::= 1 | ⟨A⟩ | (V_h , V_t)` — the unit sort, a leaf
importing a support algebra, or an ordered pair. Shapes are interned:
structurally equal shapes are one object with one `uid`.

The **unit sort** exists at one shape only and has no frontier, so no
position addresses it. It carries three roles that are one object: the
**terminal value** `1`; the **default classification**, acceptance — the
value a word earns by reaching it; and the base case of the **default
classifier**, the continuation, whose residual once every coordinate is
consumed *is* that value. The third identity is why classifying against the
continuation reproduces the normal form.

Nothing richer lives at the terminal. Counting, weighting and provenance are
classifier codomains, not terminal algebras.

A **position** is a path: a tuple of bits, `0` = head, `1` = tail, read from
the root. The frontier of a shape is its leaves in path order.

A **diagram** over a shape is, by cases on the shape:

- `None` — the adjoined zero, at every shape. Never stored inside a node,
  never traversed, never a letter.
- at `1` — the terminal `ONE`, a singleton.
- at `⟨A⟩` — a canonical, nonempty leaf code, whatever the leaf says it is.
- at `(V_h,V_t)` — a `Node`: a nonempty tuple of `(prime, sub)` pairs with
  `prime` a nonzero diagram over `V_h`, `sub` a nonzero diagram over `V_t`,
  the primes pairwise disjoint **(D)** and the subs pairwise distinct
  **(F)**, sorted by the sub's uid.

**There is one diagram type, not two.** A prime over a composite head is a
diagram over that head; the same recursion computes the support algebra of a
leaf shape (by delegation) and of a pair shape (by recursion).

Nodes are hash-consed on `(shape.uid, ((prime.uid, sub.uid), ...))`, so
equality is `is` and zero is absence. Leaf codes get uids from a side table
keyed on `(shape.uid, code)`. That uid is the only thing the kernel asks of
a code beyond hashability, and it exists to give the canonical sort a total
order.

---

## 2. `normalize` — the decomposition theorem, operationally

```
normalize(shape, rectangles) -> Node | None
```

Input: any finite list of rectangles `(prime, sub)`, overlapping and
repeating freely. Output: the unique canonical node denoting their sum.

The compilation construction of the calculus document, read literally,
enumerates sign-pattern cells and is exponential. Instead, insert
incrementally into a maintained disjoint partition:

```
cells := []                                  # invariant: primes pairwise disjoint
for (p, s) in rectangles:
    if p is zero or s is zero: skip          # smash, before anything is written
    rem := p
    next := []
    for (q, t) in cells:
        if rem is zero: keep (q,t); continue
        i := meet(V_h, rem, q)
        if i is zero: keep (q,t); continue
        r := diff(V_h, q, i)                 # relative difference, no top
        if r nonzero: keep (r, t)
        emit (i, join(V_t, t, s))            # the overlap takes the joined sub
        rem := diff(V_h, rem, i)
    if rem nonzero: emit (rem, s)
    cells := next
group cells by sub uid, joining their primes                 # (F)-compression
sort by sub uid; return None if empty, else the interned node
```

The loops are the degeneracy ledger executed: the skip is *no zero subs*,
the disjointness maintenance is **(D)**, the grouping is **(F)**, and *no
zero primes* holds because every emitted prime is a nonempty meet or a
nonempty relative difference. The all-negative cell — the one that would
need a top — is never formed, because `rem` is only ever carved out of an
input prime, never out of the ambient carrier.

`normalize`, `join` and `diff` are **mutually recursive**: normalising at a
cut joins subs one level down, which normalises at the next cut. This is the
central recursion; the memo tables hang off it, which is why they live in
one module.

---

## 3. The support algebra, uniformly

| op | at `1` | at `⟨A⟩` | at `(V_h,V_t)` |
|---|---|---|---|
| `is_zero` | diagram is `None` | code is empty (leaf decides) | diagram is `None` — a pointer test |
| `meet` | `1` | leaf meet | rectangles `(p∧q, s∧t)` over all pairs, dropping zeros, then `normalize` |
| `join` | `1` | leaf join | `normalize(a.pairs + b.pairs)` |
| `diff` | `0` | leaf relative difference | see below |

`diff(a, b)` at a pair, per `(p,s)` of `a`: carve `p` against each `(q,t)`
of `b`, emitting `(p∧q, s∖t)` for the overlaps and `(p ∖ ⋃q, s)` for what
survives. Every step is a meet or a *relative* difference; no complement and
no top is formed at any shape, which is the point of tier G.

There is no `top`, no `complement` and no preimage in this package. A leaf
may export the first two; nothing here calls them.

Emptiness at a composite shape is a pointer test *because* zero-freeness is
maintained by construction. Debug mode asserts the ledger at every
`normalize`; release mode assumes it. A release-mode failure that debug mode
would have caught is by construction a coefficient-discipline violation, so
the diagnosis is free.

---

## 4. Measurement

- `size(d)` — prime counts per depth, so the congruence tower is visible
  level by level. Summed, it is the size measure.
- `count(d)` — words denoted; a shadow operation, needing leaves that
  enumerate.
- `stats.bill()` / `stats.summary()` — call counters, `node.*` for work the
  kernel does at a cut and `<leaf>.*` for work crossing the leaf interface.
  Keeping the two apart is the point: at composite shapes most of the cost
  is the kernel's own recursion.
