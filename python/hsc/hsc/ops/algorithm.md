# hsc/ops — algorithms

Homomorphisms: terms with a code action. Three families — local homs,
combinators, and classifier-driven homs — plus the two open questions this
package is where we will have to answer.

---

## 1. Application

A hom is a frozen, hashable object with an `_apply(shape, diagram)`, and
application is cached on `(hom, shape.uid, diagram uid)`. Terms themselves
are *not* canonicalised — see §4 — so only applications are shared, and the
cache does not care.

Every `_apply` rebuilds through `normalize`, so canonicity is restored after
each action rather than assumed to be preserved by it.

---

## 2. Local homs

A local hom addresses one frontier position. The recursion is the point:

> **A local hom at a cut is the same hom one level down**, applied to the
> primes when the path turns into the head and to the subs when it turns
> into the tail.

Recursing by re-invoking the hom, rather than by calling a helper, is what
keeps the traversal shared. A hom that recursed through a plain function
would walk the diagram's *tree unfolding* instead of its DAG, discarding
exactly the sharing that makes the representation worth having. Measured on
the philosophers: making this change turned leaf traffic from superlinear
into exactly linear in the number of philosophers, and made one full step on
a fixpoint cost zero uncached operations.

- `Filter(path, code)` — meet the leaf at `path` with `code`. No cylinder
  over the other coordinates is built, so no top is needed anywhere: the two
  faces of a support are kept apart on purpose, and the bridge between them
  is the tier-B purchase this kernel declines to make.
- `Write(path, code)` — overwrite one coordinate with a constant.
  Memory-destructive, so previously distinct primes may collide; the
  collision is precisely what re-normalisation merges, and nothing special
  is needed for it.
- `Assign(writes)` — the parallel form, the primitive. Sequential writes
  cannot express a simultaneous exchange without temporaries. With constant
  right-hand sides the writes commute, so their order is immaterial.

---

## 3. Classifier-driven homs

Each is `split_equiv` plus a decision about the pieces, and nothing here
re-implements the traversal.

- `Guard(expr)` — keep the accepting piece. Over one coordinate it does not
  travel and is the plain filter; over several it curries.
- `Put(path, expr)` — split by the value of the right-hand side, write that
  constant on each piece, re-fuse.
- `Case(expr, branches)` — quotient, act per class, re-fuse. Each branch is
  invoked **once per congruence class**, not once per element and not once
  per declared letter: the branch set is discovered, minimal and canonical.
  A letter with no branch is dropped unless a default is given; a branch
  with no realised letter is never invoked.

---

## 3b. Blocks travel; leaves get soup

*The intended architecture. The code does not do this yet: `Compose`
currently applies its parts one after another, which is n full traversals of
the diagram where this design is one.*

**A block is a whole term, not a chain.** What travels is a term over
filters, assigns, composition, **sum**, and **star closure** — not merely a
`∘`-chain. That matters at the leaf, which is handed the entire local term
and may fuse all of it: for a BDD theory, `∘` is relational composition, `+`
is disjunction of relations and `*` is transitive closure, all native.

**`skip` is compositional, uniformly.** A block skips a level exactly when
every part does, and that rule is the same for `∘`, `+` and `*`. So a
block's footprint is derivable from its parts' and the block descends **as
one object** for as long as it skips. Support is not primarily a hint for a
schedule; it is what decides how far a term travels before it has to act.

**A block parenthesises at the level it cannot skip**, splitting along the
two congruence directions: the part acting on the head goes to the *edge*,
applied to the primes; the part acting on the tail goes *down*, applied to
the subs; the node rebuilds through `normalize`. This is the node
materialising its own congruence and deferring the other, seen from the
operator side instead of the data side.

```
apply(block, shape, d):
    if every part skips this shape:  return d                    # travel through
    if shape is a leaf:              return leaf.apply_local(d, block)
    split block across the cut -> (edge parts, tail parts, crossing parts)
    for (p, s) in d.pairs:  p' = apply(edge parts, head, p)
                            s' = apply(tail parts, tail, s)
    normalize
```

Memoised on `(block, shape, diagram)`; the split is cached per
`(block, shape)`. Single-position homs are the degenerate case, a block of
one.

**Only `∘` splits across a cut.** A composition of parts with disjoint
supports is a tensor and factors into edge and tail. A **sum does not**:
`(h_e ∘ h_t) + (g_e ∘ g_t)` is not `(h_e + g_e) ∘ (h_t + g_t)`. A sum
therefore travels intact only while its summands skip *together*, and splits
into separate applications joined at the level where they part company.

**A star travels to the lowest cut its support touches, and is applied there
as a local fixpoint, memoised per node — which is the kernel of
saturation.** So saturation is not wholly a separate scheduling question
bolted on afterwards: putting `*` in the block algebra and letting blocks
travel delivers its core, and what remains genuinely open (§5) is the order
between local and crossing events and the footprint notion, not the idea
itself.

**A leaf is handed a maximal local block, not an opcode.** The framework's
obligation stops at delivering the whole composition whose support lies in
one leaf; what the leaf does with it is the theory's business. A BDD theory
fuses guard and update into one relation over interleaved current/next
variables and applies it with a single relational product; a finite
enumerated theory simply executes the parts. The normal form for
compositions is therefore **per theory**, and the framework must not
prescribe one.

Splitting a block is licensed by parts with disjoint support commuting —
the same independence partial-order reduction rests on. That is why §4 and
§5 below are one question and not two: the rules that let a block reorder
and fuse are the rules that let it split at a cut and travel.

A part whose support *crosses* a cut can go to neither side. That is
precisely and only where `split_equiv` fires — the genuinely non-Kronecker
case, a guard or an assign relating coordinates in different leaves. A
system whose every event is a tensor of local actions never reaches it,
which is why a Petri-net-like protocol is entirely blocks travelling and
leaves fusing, with the quotient constructor never invoked. That fragment is
the declared regime of §11, named there as exactly the one in which `Θ` is
never invoked.

---

## 4. Open: the term normal form

The calculus document canonicalises an elementary term to a strictly
alternating guarded word, merging adjacent filters by meets. That is
canonical and **anti-optimal**, and the document's framing of it
over-assumes: merging two single-coordinate guards produces a
two-coordinate guard that must now travel, where separately they never
would.

The shape of the right answer is not a canonical word but a **confluent
rewriting system** whose rules consult commutativity and the *relative
support* of operands to decide reordering. Independence of supports is the
same notion partial-order reduction uses to justify not exploring both
interleavings of independent events; here it justifies commuting, merging or
refusing to merge two terms. The reordering such a system enacts is also
what a schedule needs (§5), so these are one problem seen twice, not two.

This is the package's principal open question and it is a theory question
before it is a coding one.

---

## 5. Saturation

`Star(h)` is the least fixpoint of `X -> d + h(X)`. Monotonicity makes every
fair schedule agree on the value, so scheduling lives strictly below the
semantics -- but not below the *cost*: iterating by rounds rebuilds a fresh
diagram each time, so every round misses the caches and the cost is
rounds x events x |X| in freshly built nodes. Saturation is the answer, and
with a shape tree it needs no declaration: the split below is **derived from
supports**.

### The decomposition at a cut

Write the term being saturated as

```
H = (L + G + F + id)*
```

split by where each summand's support lies relative to this cut:

| part | support | disposition |
|---|---|---|
| `F` | tail only | skips this level; belongs downstream |
| `L` | head only | local to this level's edge |
| `G` | crosses the cut | anchored here -- an operation's top is the highest level its support reaches |

`L`, `G` and `F` are each sums of compositions, and `id` sits in the sum so
each closure is reflexive.

### Throwing a crossing event in two directions

A `g` in `G` that is Kronecker factors as `g = l o f`, a local part and a
tail part. It then does **not** have to act at this level. Instead it is
thrown both ways, with the re-saturation fused into its own application so
that nothing is left unsaturated behind it:

```
down the tail:   (F + id)* o f
on the edge:     (L + id)* o l
```

**Selections first, on both sides.** `g` fires only if both members find
friends. Launching `(F + id)*` -- an unbounded fixpoint -- before testing
that `l` has a match is speculative at best and disrupts the semantics at
worst, since the other member may return the empty set. Apply the guards of
both members, drop the pair if either is empty, and only then apply the
updates and re-saturate.

### The schedule at a node

```
saturate(H, shape, d):
    if shape is a leaf:                       # the theory closes its own term
        return leaf.apply_local(d, H)
    d := (F + id)*  on the subs                          # downstream first
    d := (L + id)*  on the primes                        # then local
    repeat until d stops changing:
        for g in G:                                      # chaining, not a joint fixpoint
            d := (g + id)(d)                             # thrown both ways, as above
```

`(F + id)*` runs before the first `g` is ever touched, and the local
saturation runs too. The `G` loop then **chains**: each `g` is applied to
the accumulated `d` in turn, rather than the whole sum being iterated
jointly.

`g(d)` at a node with pairs `(p, s)` is the tensor: `((L+id)* o l)` on `p`
and `((F+id)* o f)` on `s`, rebuilt through `normalize`.

The recursion is what makes it hierarchical -- the two child saturations are
this same procedure one level in, bottoming out where a theory closes its
own local term. Nothing is declared; the split falls out of the supports.

### What remains genuinely open

- **The footprint notion.** `support()` is a static over-approximation, the
  complement of libDDD's `skip`, but `skip` is *minimal* and may be
  *dynamic*: an operation reading `tab[x]` has a static support covering all
  of `tab` while its actual footprint depends on the data. Static support is
  adequate only while every footprint is static, which is true of everything
  in this prototype today and will not stay true.
- **Non-Kronecker crossing events.** A `g` that does not factor as `l o f`
  cannot be thrown in two directions; it has to quotient. See §6.

Groundwork present: `Hom.support()` and `Hom.rerooted(bit)` on every hom.
The procedure above is not implemented yet.

---

## 6. The general case: a crossing event that does not factor

When `g` in `G` is not Kronecker, its two ends are *correlated* -- which
value the tail end applies depends on what the edge end saw, or the reverse.
The level can no longer relink blindly, so it quotients. This is
`case` at a cut, with the branch table not declared but *discovered*: the
branches are the curried residuals of `g` itself.

Three dispositions, and choosing between them is a cost decision.

**Split the edge, specialise the tail.** Grab the value the operation needs
from a coordinate in the head, `split_equiv` the edge on it to build the
equivalence classes, and for each class apply the correspondingly curried
operation to the tail. Then relink each edge class to the tail that received
*its* class's operation:

```
for (p, s) in d.pairs:
    for residual, p_class in split_equiv(head, p, classifier of g):
        s_class = apply(g curried by residual, tail, s)
        emit (p_class, s_class)
normalize
```

The edge classes are disjoint, so (D) survives for free; two classes whose
specialised operations happen to agree produce equal tails and (F) merges
them. Canonisation does the rest -- nothing here maintains the invariants by
hand.

**Split the tail, specialise the edge.** The mirror image: quotient the tail
to determine the classes, apply a different treatment to the edge per class,
relink, canonise. Here the resulting edge parts may *overlap* rather than
partition, which `normalize` also absorbs, unioning the subs of the pieces
that coincide.

**Split both, congruently.** When both ends read as well as write -- a guard
relating a head coordinate to a tail coordinate is the archetype -- both
sides are quotiented and the classes are matched by residual, only the
agreeing pairs surviving.

The two directions are the two congruence directions of the decomposition
theorem, seen from the operator side: the node materialises one of them and
defers the other, and which one it materialises is exactly the choice above.
Neither is canonical; the cost model decides, by the same three factors that
price any other query -- how fast the residuals separate, how far they
travel, and how cheaply the classes can be named.
