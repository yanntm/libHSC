# handoff — parametric surface

Spec: `research_notes/hsc_parametric.md`. Report:
`research_notes/hsc_parametric_report.md`. M1 (expander) and M2 (samples)
are done and green; the queue:

## Engineering
1. **M3 layout experiment** — philosophers (scale N up from 5) at spine vs
   balanced vs grain-blocked shapes; nodes / peak / time per layout, into
   the report. This decides whether a block-layout sort sugar is worth
   adding.
2. **Multi-dim arrays** (spec §3, design settled): `(array m N M [LO HI])`
   → cells `m_i_j`; `(at m I J)` grounded → cell, open → row-major
   linearized 1-D access. Expander only.
3. On demand: `-DN=` CLI override of `param`.

## Theory
1. Rule on the `select` seam (report finding 1): narrow the doc's
   `QATOM ::= BEXP`, or spec routing rich BEXPs through the case engine.
2. Scalarset discipline + orbit tags (spec §3): the checkable symmetric
   subset and what quotienting would consume — paper-sized, unblocked by
   nothing.

## Blockers
None.
