# handoff — parametric surface

Spec: `research_notes/hsc_parametric.md`. Report:
`research_notes/hsc_parametric_report.md`. M1 (expander) and M2 (samples)
are done and green; the queue:

M1, M2, M3 done — see the report, whose M3 section carries the headline:
the diagram side is O(log N) on balanced shapes (+4 nodes per doubling of
N), the operation side is Θ(N) (op terms pinned at 6 per instance, no
cross-instance sharing — terms address absolute frontier positions).
2^n-philosophers-in-O(n) is blocked there and only there.

## Engineering
1. **Profile the superlinear translator/engine cost** (×4 N → ×12 time at
   N=16384, net of the expander) — a per-event factor growing with the
   frontier. Find it, name it, fix or file it.
2. **Log-form / exact cardinality**: `cardinal` is a double, exhausted at
   ~1e15 states; cross-layout checks already disagree in low digits at
   1e97. Needed before any large-N claim.
3. On demand: `-DN=` CLI override of `param`.

## Theory
1. **The 2^n construction** (promoted from "later" — now measured and
   motivated, report M3): spec translation-invariant event terms
   (relative addressing; `read_when`'s shift-to-0 of guard bexprs is the
   existing seed) and a repetition/induction combinator over the interned
   sort DAG — 3 templates × n levels = O(n) codes, an O(1) seam per
   level. The substrate is ready; the term algebra is the gap.
2. Rule on the `select` seam (report finding 1) — resolved in code: select
   atoms now take any BEXP through the guard path; bless or amend the
   doc's `QATOM ::= BEXP` accordingly.
3. Scalarset discipline + orbit tags (spec §3), now feeding item 1.

## Blockers
None.
