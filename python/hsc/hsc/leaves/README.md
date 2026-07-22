# hsc/leaves — imported support algebras

A leaf module implements `core.leaf.Leaf` up to its declared tier and
nothing above it. The kernel only ever compares codes (equality, emptiness)
or hands them back to their module (meet, join, relative difference).

| module | carrier | tier | notes |
|---|---|---|---|
| `enum.py` | an explicit finite set | B | codes are frozensets; call-counted, and `bill()` returns the invoice |
| `bdd.py` | a block of Boolean bits | B | codes are BuDDy BDDs, value-equal and value-hashable; one shared variable block for every position |

`bdd.py` never calls `bdd_init` unguarded: a second call aborts the process
rather than raising, and importing Spot initialises BuDDy. Use
`ensure_manager()`. Codes are the SWIG `bdd` proxies themselves, which
refcount the underlying node (`bddx.h`, copy-ctor and dtor), so a code the
kernel holds cannot be collected under it.

The tier ledger, the `split_equiv` contract and what a new theory module
owes are in `algorithm.md` in this folder. One module per theory.
