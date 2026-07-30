# Handoff — CEGAR (certified component abstraction)

Paper: `research_notes/cegar_certified_abstractions_v2.md`. Spec:
`research_notes/cegar_spec.md` (v1: finite instance). Report:
`research_notes/cegar_report.md`. Code: `include/hsc/cegar/` +
`src/cegar/` (lib `hsc_cegar`), tools in `tools/cegar/`, tests in
`tests/cegar/` (own binary), campaign in `experiments/cegar/`.

State: v1 delivered, M0–M6 green (18 cases / 1210 assertions), full
campaign clean — 212 models, 0 parity disagreements, 56/56
certificates pass the independent checker, 0 budget violations.

## TODO — theory (next action first)

1. Read the report's findings F1 (initial hypothesis must be
   published as chaos — spec §3.2 permits an unsound reading; revise)
   and F2 (paper §6's "clients never touched" is search-order
   dependent; restate around entry-count locality) and fold into spec
   and paper.
2. T-D is uninformative: the rand corpus is violation-dominated
   (policies never diverge). Design a refinement-heavy corpus (holds
   instances with deep spurious chains, or bug depth as a generator
   parameter) and add the expected-outcome section to spec §10.
3. Decide the next relaxation to spec (paper §8 order: interior
   contracts first).

## TODO — engineering

1. Nothing blocking. On spec revision for T-D, extend `gen` and
   re-sweep.
2. Candidate cleanup when touched next: `hsc-cegar run` could report
   per-entry rungs (name → index) for cone-of-influence tables.

Blockers: none.
