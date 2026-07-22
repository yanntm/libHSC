# hsc — Hierarchical Shape Calculus, prototype

Implementation of the calculus described in `research_notes/hsc_core.md`,
along the line of `research_notes/hsc_spec.md`. Performance is not a goal;
the products are the API, the measured cost model, and pressure on the
theory's open obligations.

## Layout

- `hsc/core/` — shapes, diagrams, the canonical decomposition, the support
  algebra.
- `hsc/classify/` — the classifier language and `split_equiv`, the
  traversal backbone.
- `hsc/ops/` — homomorphisms: local, combinators, classifier-driven.
- `hsc/leaves/` — one module per imported theory.

Each has a `README.md` (source map) and an `algorithm.md` (the algorithms,
written before the code). `algorithm.md` at this level is the index and
states the invariants that cross packages.
- `hsc/model.py` — modelling sugar: name leaves, build a shape, resolve
  names to frontier paths.
- `examples/` — runnable systems. `counter.py` (4-bit increment),
  `philosophers.py` (Ex1 of the calculus document).
- `tests/` — property checks against the brute-force shadow.

## Quick start

```
python -m examples.philosophers      # from hsc/
python -m examples.counter
python -m tests.test_algebra
```

## Scope of this iteration

Deliberate simplifying assumptions, each one a named place to grow:

| assumption | where it bites | lifted by |
|---|---|---|
| `Star` is plain BFS | fixpoint cost is rounds x events x \|X\|, not O(representation) | saturation — needs a theory round first, it is not in the calculus document |
| `support()` is static | over-approximates as soon as an operation reads an index it computes | a minimal, possibly dynamic `skip` |
| no term normal form | operation terms do not dedupe; their applications do | a confluent rewriting system over commutativity and relative support |
| one leaf theory (`enum`) | every split is an enumeration, so the third cost factor is nominal | a theory whose codes are structured |
| no s-expression surface | models are built in Python | later, once the classifier seam settles |

Not on the plate, on purpose: **inverse images** (undoing a destructive
assignment needs a reachable set to move within — a different problem), and
**weights at the terminal** (counting, weighting and provenance are
classifier codomains, discovered per query; the calculus does not grow a
terminal algebra to express what a classifier already expresses).

What *does* run end to end: shapes with composite heads, so internalization
is exercised from the first line; the canonical decomposition; the full
support algebra with no top anywhere; filters, parallel constant assigns,
sum and star — enough to compute reachable sets and inspect their congruence
towers; and `split_equiv` with its callers — multi-coordinate guards,
computed assigns, `case`, and the quotient constructor with discovered
alphabets and the kernel merge.
