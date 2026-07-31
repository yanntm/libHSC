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
metrics, falsifiable expectations; citations landed with a References
section, every entry read from source (`cegar_citations.md` is the
registry: keys, holdings, verification notes, and the do-not-cite
queue — priority: AGAR, CAV 2008, the nearest cousin, read before the
contribution claim is final; then AG origins, BEEM/MCC infra, learner
engineering, plus exact venues for four preprint-copy entries).
Drafts v1/v2 are superseded, retire when convenient.

## TODO — theory (next action first)

1. Revise spec §3.2 to state the publication discipline (F1) —
   currently the spec still permits the unsound reading the paper's
   Remark 3.7 now forbids.
2. T-D is uninformative: the rand corpus is violation-dominated
   (policies never diverge). Design a refinement-heavy corpus (bug
   depth / spurious-chain depth as generator parameters); spec §10
   revision (mirrored as paper §10.5).
3. Decide the next relaxation to spec (paper §10 order: interior
   contracts first).
4. **CAC08 subjects 8 and 17** (Chiron property 8) exist only as QRE +
   Ada — no FSP in the artifact. Hand-translate from QRE, or drop them
   and state the gap? The other 30 subjects are in hand.

## TODO — engineering

1. **CAC08 corpus**: acquisition done — the authors' own artifacts are
   in `examples/cac08/` (`upstream/` tarballs, `curated/` working set),
   so the reconstruction spec is deleted and we translate the real
   subjects. Next: an FSP `.lts` reader → `.cts`, parameterized subset
   first (Gas Station / Peterson / Relay / Smokers), then Chiron's
   pre-flattened files. Then the teacher-cost split in the loop's
   output if absent. See `cegar_report.md` §CAC08.
2. On spec revision for T-D, extend `gen` and re-sweep.
3. Candidate cleanup when touched next: `hsc-cegar run` could report
   per-entry rungs (name → index) for cone-of-influence tables.

Blockers: none.
