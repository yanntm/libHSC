# hsc/classify — the traversal backbone

Read `algorithm.md` in this folder first.

| module | responsibility |
|---|---|
| `expr.py` | the classifier language: interned expressions carrying their own rebuilder |
| `query.py` | `split_equiv`, `Partition` (kernel + labelling), the federating merge, `theta` |

`split_equiv` is the one traversal. Guards, computed assigns, `case` and the
quotient constructor are its callers, differing only in the codomain of the
classifier and in what they do with the returned pieces; nothing outside
this package re-implements the traversal.

Depends on `core` only. Knows nothing about homomorphisms.
