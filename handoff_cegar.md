# Handoff — CEGAR (certified component abstraction)

Paper: `research_notes/cegar_certified_abstractions_v3.md`. Spec:
`research_notes/cegar_spec.md` (v2: finite instance, HSC-native).
Report: `research_notes/cegar_report.md`. Code: `include/hsc/cegar/`
(core) + the surface bridge/checker; commands `cegar` /
`certificate` / `certcheck` in the main, `(input FILE)` for the
model/driver split; tests in `tests/cegar/`, examples in
`examples/cegar/`, campaign in `experiments/cegar/`.

State: **HSC-only port delivered** — one language, one parser; side
formats, binaries and the generator are gone from the tree. Tests 18
cases / 1205 assertions green; T-A and T-C regenerated on the family
corpus, all rows agree across the cegar/symbolic/explicit triangle,
all holds certified in-sweep; ring refinement measured flat in N
under interning; the teacher-cost split (search/replay/refine ns) is
a column everywhere. Latent uniform-family crash found and fixed
(report F3). Random corpus dropped from the campaign (fuzzing lives
in the test suite).

Paper v3: standalone, citations landed from source
(`cegar_citations.md` is the registry; priority acquisition: AGAR,
CAV 2008, before the contribution claim is final). Drafts v1/v2
superseded, retire when convenient.

## TODO — theory (next action first)

1. **CAC08 subjects 8 and 17** (Chiron property 8) exist only as QRE +
   Ada — no FSP in the artifact. Hand-translate from QRE, or drop them
   and state the gap? The other 30 subjects are in hand.
2. T-D needs its corpus: the families resolve in too few rounds for
   policies to diverge. Design the refinement-heavy family
   (spurious-chain depth as a parameter), as committed `.hsc` models
   or a spec'd generator; spec §10 revision (mirrored as paper §10.5).
3. Decide the next relaxation to spec (paper §10 order: interior
   contracts first).

## TODO — engineering

1. **CAC08 translation**: FSP reader emitting separable-fragment
   `.hsc` (model-only files; drivers via `(input …)`), parameterized
   subset first (Gas Station / Peterson / Relay / Smokers), then
   Chiron's pre-flattened files. Artifacts in `examples/cac08/`
   (`upstream/` tarballs, `curated/` working set). See
   `cegar_report.md` §CAC08.
2. Candidate cleanup when touched next: the `cegar` summary could
   report per-entry rungs (name → index) for cone-of-influence
   tables.

Blockers: none.
