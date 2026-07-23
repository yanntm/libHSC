# handoff — parametric surface

Spec: `research_notes/hsc_parametric.md`. Report:
`research_notes/hsc_parametric_report.md`. M1 (expander) and M2 (samples)
are done and green; the queue:

M1, M2, M3 done — see the report, whose M3 section carries the headline:
the diagram side is O(log N) on balanced shapes (+4 nodes per doubling of
N), and the operations *already share across instances* (the case engine
re-roots on descent; verified: op terms = 2N + ~30 per family, the ~30
being the shared operations). The Θ(N) remainder is pure site
enumeration — N wrapper chains around one shared term — and it folds, by
distributivity, into a log-depth recurrence. 2^n-philosophers-in-O(n) is
blocked there and only there.

## Engineering
1. **Profile the superlinear translator/engine cost** (×4 N → ×12 time at
   N=16384, net of the expander) — a per-event factor growing with the
   frontier. Find it, name it, fix or file it.
2. **Log-form / exact cardinality**: `cardinal` is a double, exhausted at
   ~1e15 states; cross-layout checks already disagree in low digits at
   1e97. Needed before any large-N claim.
3. On demand: `-DN=` CLI override of `param`.

## Theory
1. **The 2^n construction** (promoted from "later" — now measured, and
   narrowed by the probe, report M3): translation invariance is done
   (re-rooting); what needs spec is the wrapper factorization
   `node(A,id) ⊕ node(B,id) ≡ node(A⊕B,id)` as a sum normal form or a
   declared-family recurrence `R(d) = node(R(d−1),id) ⊕ node(id,R(d−1))`
   built over the sort DAG from the binder — plus the schedule's
   consumption of factored terms (fixpoint cached per shared
   operation × shared block). Circular-set constraints (uniform
   CUR/NEXT, no symmetry-breaking index) are the checkable precondition
   for the declared route.
2. Scalarset discipline + orbit tags (spec §3), now feeding item 1.

The select seam is closed: atoms take any BEXP through the guard path,
so the documented `QATOM ::= BEXP` is now literally true. The paper
(hsc_core5) carries the fold as Proposition 4.5, the measured §8
witness, and Open 6 (regular families) — the theory queue's item 1 is
that Open item's working half.

## Blockers
None.
