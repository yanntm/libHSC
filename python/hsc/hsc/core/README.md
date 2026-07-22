# hsc/core — the kernel

Read `algorithm.md` in this folder before editing anything here; the code
is meant to be its transcription. `../../algorithm.md` gives the
system-wide picture and the invariants that cross packages.

| module | responsibility |
|---|---|
| `shape.py` | interned shape trees, frontier paths, name resolution |
| `leaf.py` | the leaf interface: the tier ledger as an ABC, plus `TierError` |
| `diagram.py` | `Node`, the unique table, uids, the adjoined zero |
| `algebra.py` | `normalize` (Theorem 5.1) and the support algebra; mutually recursive, hence one module |
| `stats.py` | the invoice: call counters, split by side of the leaf interface |

Invariants the kernel maintains and never re-derives:

- zero is `None` at every shape; it is never stored in a node, never
  traversed, never sorted;
- nodes are unique-tabled, so equal diagrams are the same object and
  emptiness at a composite shape is a pointer test;
- primes over a composite head are diagrams over that head — there is one
  diagram type, and internalization is the class hierarchy;
- no `top` and no `complement` is called anywhere; every carving is a meet
  or a relative difference against a prime already present;
- there is no inverse image, absolute or relative, and none is planned.

`algebra.DEBUG` toggles the degeneracy-ledger assertions on every
constructed node. A release-mode failure that debug mode would have caught
is by construction a coefficient-discipline violation.
