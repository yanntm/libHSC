# Handoff — CEGAR (certified component abstraction)

Paper: `research_notes/cegar_certified_abstractions_v2.md`. Spec:
`research_notes/cegar_spec.md` (v1: finite instance). Report:
`research_notes/cegar_report.md`. Code: `include/hsc/cegar/` +
`src/cegar/` (lib `hsc_cegar`), tools in `tools/cegar/`, tests in
`tests/cegar/` (own binary), campaign in `experiments/cegar/`.

State: v1 delivered, M0–M6 green (18 cases / 1210 assertions), full
campaign clean — 212 models, 0 parity disagreements, 56/56
certificates pass the independent checker, 0 budget violations.

Paper is at **v3** (`cegar_certified_abstractions_v3.md`):
standalone, F1/F2 folded in, T3 restated on the budget pool,
prospective evaluation section (§9) sketching corpora, comparators,
metrics, falsifiable expectations. Drafts v1/v2 are superseded,
retire when convenient.

## TODO — theory (next action first)

1. Acquisition pass on `cegar_citations.md`: pull the
   [TBD: check from source] entries into `papers/` (priority: the
   learned assume-guarantee line — Cobleigh et al., "breaking up is
   hard to do" — and CEGAR/lazy abstraction), read, then let §9's
   framing cite them. Verify the flagged BtL identifications
   (Arnold 1985, Abdulla 2011, Chen–Liu 2017) from source.
2. Revise spec §3.2 to state the publication discipline (F1) —
   currently the spec still permits the unsound reading the paper's
   Remark 3.7 now forbids.
3. T-D is uninformative: the rand corpus is violation-dominated
   (policies never diverge). Design a refinement-heavy corpus (bug
   depth / spurious-chain depth as generator parameters); spec §10
   revision (mirrored as paper §10.5).
4. Decide the next relaxation to spec (paper §10 order: interior
   contracts first).

## TODO — engineering

1. Nothing blocking. On spec revision for T-D, extend `gen` and
   re-sweep.
2. Candidate cleanup when touched next: `hsc-cegar run` could report
   per-entry rungs (name → index) for cone-of-influence tables.

Blockers: none.
