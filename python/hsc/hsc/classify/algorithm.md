# hsc/classify — algorithms

The classifier language and `split_equiv`, the traversal every construct
that inspects data is built from.

---

## 1. Classifiers

A classifier is an interned expression over leaf coordinates. Two properties
matter, both structural:

- **interned** — structurally equal expressions are one object, so an
  expression *is* its canonical code and equality is a pointer test;
- **normalising while it travels** — an applied operation carries the
  *builder* that made it, and substitution rebuilds through that builder.

The second is the one that is easy to get wrong. Rebuilding through a
generic constructor normalises an expression when a client writes it and
never again, which is precisely backwards: currying is when residuals get
their chance to coincide, so that is when normalisation has to happen.

Normalisation strength is a **cost parameter, never a soundness one**. An
equality a leaf fails to detect costs redundant subqueries, which the merge
of §3 recovers on the way back up. Nothing may be written that assumes a
particular strength.

---

## 2. `split_equiv`

```
split_equiv(shape, d, expr) -> Partition of d
```

Partition `d` into the pieces on which `expr` has each realised residual.
Zero pieces are absent, so the label set *is* the discovered alphabet: empty
classes are never represented and pieces that agree are merged before any
client sees them. A ground label is a discovered letter; a non-ground one is
a cut the classifier has not finished crossing.

**Down.** Travel to the first coordinate the classifier mentions. A
classifier mentioning nothing in a subtree returns there in one entry
without descending: locality is the distance-zero case and it is free.

**Across.** At a leaf, ask the leaf to split its code by the expression.
This is where a leaf earns its keep — returning one structured code where
enumeration would return many singletons — and where the third cost factor
lives. Meeting a coordinate substitutes its class and renormalises, so the
travelling classifier stays ground and mentions only coordinates not yet
consumed; a consumed coordinate can never be re-queried. Because
expressions are interned, grouping head-classes by residual code and keying
the memo table on that code are the same act: deduplication *is* cache
sharing.

**Up.** Results federate; see §3.

At a cut the three movements compose as: split the head, curry each residual
into the tail, and reassemble by (F)-compression through `normalize`.

---

## 3. Partitions, kernels and the merge

A `Partition` is a **kernel** — the canonical, interned tuple of pieces —
plus a **labelling** from residuals into it. Splitting the two is what lets
partitions federate: agreeing on the kernel is a pointer test, and
disagreeing on the labelling costs a finite table.

Every partition is federated, including the ones that did not descend and
the ones that turned out to have a single class. Not descending is a
locality optimisation; not federating is an error, because it would let two
equal partitions carry different kernel ids and the merge would stop being
an equivalence.

One-class partitions therefore come in two kinds, same kernel, different
labelling:

- labelled by a **value** — the terminal case; with the value `1`, this is
  acceptance, the default classification;
- labelled by a **residual expression** — a suspended computation: nothing
  here separates anything, ask below.

The merge is where weak leaf normalisation is repaid retroactively: two
residuals whose codes never coincided but whose realised partitions do meet
here and share one kernel. It is also where the equivariant collapse
appears — not as a code path, but as what the merge finds when the structure
is there.

**Measured.** `(b+c) mod 3` over ranges 10 / 30 / 90 gives an identical bill
every time: three residual codes at the first coordinate, eleven subqueries,
seven kernels of which five separate anything. The three residuals reaching
the `⟨c⟩` sort induce the *same* three pieces with permuted labels — one
kernel, three tables — with nothing in the kernel aware of what `mod` is.

---

## 4. `theta`

`Θ` is `split_equiv` with the labels required ground: discovered letter to
the part it classifies. Every class is nonempty and no two classify the same
part, so the alphabet is the coarsest refinement compatible with the
classifier on the data — minimal and forced, inherited from the
decomposition theorem rather than arranged here.

Reporting helpers `kernel_report` and `kernel_detail` exist so that the cost
model is measured rather than assumed; `kernel_detail` breaks the harvest
down per sort, which is where a collapse is legible.
