# hsc/leaves — algorithms

A leaf is a *theory*: an external domain imported through a finite window.
One module per theory. The window, not the domain, is what the calculus
sees, and a leaf's job is to make the window narrow and the codes small.

---

## 1. The tier ledger

Cumulative; a leaf declares its tier and implements up to it, and the kernel
calls only what the operation at hand consumes.

| tier | adds | used by |
|---|---|---|
| E | canonical codes, equality, emptiness | every decision the kernel makes |
| J | finite joins | assembling sections, `(F)`-compression |
| G | finite meets, **relative** differences | maintaining `(D)` inside `normalize` |
| B | top, complement | nothing in the kernel |

Tier G is the working tier and deliberately has **no top**. A leaf without a
top is legal and expected.

**There is no preimage in this interface, absolute or relative, and none is
planned.** Undoing a destructive assignment needs a reachable set to move
within; that is a different problem for a different iteration, and the
interface must not grow a hole for it in the meantime.

---

## 2. `split_equiv` — where a leaf earns its keep

```
split_equiv(code, expr) -> {residual expression : nonempty piece}
```

Partition the code by the residual of the classifier after substituting this
leaf's coordinate.

Enumerating the carrier is a legal implementation and is what `enum.py`
does, because its carrier is explicitly finite and enumeration is honest
there. It is also the *only* thing a leaf can do badly. A leaf earns its
keep by returning few, structured codes where enumeration would return many
singletons — that ratio is the third factor of the cost model, and it is the
one factor decided entirely by the leaf.

A leaf may also normalise the classifier terms it understands. That strength
is a **cost parameter, never a soundness one**: an equality it misses costs
redundant subqueries which the kernel merge recovers retroactively.

---

## 3. Contract for a new theory module

- codes are hashable and canonical — two codes denoting the same set must be
  equal, since the kernel's canonical sort keys on them;
- `is_empty` is total and exact; it is the one semantic decision canonicity
  leans on;
- `diff` is the *relative* difference;
- every method is call-counted through `core.stats`, because the bill is a
  product of this repository and not telemetry;
- `elements` is the shadow's door, for oracle tests over small carriers
  only; nothing in the kernel calls it.

---

## 4. Modules

| module | carrier | tier | notes |
|---|---|---|---|
| `enum.py` | an explicit finite set | B | codes are frozensets; splits by enumeration, which is honest for a finite carrier and is the baseline every other leaf should beat |
| `bdd.py` | a block of Boolean bits | B | codes are BuDDy BDDs; currified variables shared by every position; `split_equiv` not yet implemented |

## 5. Currified variables

A theory whose codes are relative rather than absolute lets isomorphic
positions share them. `bdd.py` allocates **one** variable block, indexed from
the terminal upward, and every leaf of that theory uses it: a local state is
one node whatever position holds it, and the manager never learns how many
positions there are.

This is the "no names" discipline, not a space optimisation -- positions are
paths, and two equal subterms are one object. It is also where the calculus
parts company with vtree-based decision forms, in which every variable occurs
at exactly one place and therefore no two positions can share a code.

Measured: dining philosophers over BDD leaves keeps the BuDDy manager at 32
nodes for 2, 4 and 8 philosophers, while the shape's own tower grows.

## 6. Exported maps, and local terms

Beyond the support-algebra tiers, a leaf exports *maps* -- the raw material
for assigns. A structured carrier needs them: writing one bit of a block is
not a constant, it is `exists bit . code` conjoined with the new value, the
pushforward of a partial assignment, memory-destructive exactly as an assign
should be.

The interface a theory should ultimately be asked for is not one map at a
time but a **maximal local term**: the whole term -- guards, assigns,
composition, sum, star closure -- whose support lies in this leaf, handed
over for the theory to interpret as it likes. A BDD theory reads all of it
natively: composition is relational composition, sum is disjunction of
relations, star is transitive closure, and the result is one relation over
interleaved current/next variables applied with a single relational product.
A finite enumerated theory just executes the parts. **The normal form for a
local term is per theory; the framework delivers the term and does not
prescribe the fusion.** Splitting a code, acting per piece and re-joining is
what a theory with a native operation must never be made to do.

Relational products are also why `split_equiv` is *not* the mechanism for
operations. It is the mechanism for **classifiers** -- queries whose value
accumulates across leaves, and guards or assigns that genuinely relate
coordinates in different leaves. Where an event is a tensor of local
actions, no classifier travels and none should.
