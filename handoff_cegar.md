# Handoff — CEGAR (certified component abstraction)

Paper: `research_notes/cegar_certified_abstractions_v3.md`. Spec:
`research_notes/cegar_spec.md` (v2: finite instance, HSC-native).
Report: `research_notes/cegar_report.md`. Code: `include/hsc/cegar/`
(core) + the surface bridge/checker; commands `cegar` /
`certificate` / `certcheck` in the main, `(input FILE)` for the
model/driver split; tests in `tests/cegar/`, examples in
`examples/cegar/`, campaign in `experiments/cegar/`.

State: **v2 delivered, HSC-native** — one language, one parser.
Tests 18 cases / 1205 assertions green; T-A and T-C on the family
corpus, all rows agree across the cegar/symbolic/explicit triangle,
all holds certified in-sweep; ring refinement flat in N under
interning; the teacher-cost split (search/replay/refine ns) is a
column everywhere. Random-model fuzzing lives in the test suite; the
campaign measures the families.

Paper v3: standalone, citations landed from source
(`cegar_citations.md` is the registry; priority acquisition: AGAR,
CAV 2008, before the contribution claim is final).

## TODO — theory (next action first)

1. **The CAC08 cegar-timeout pattern** (report §CAC08): 14 subjects
   where cegar blows 15 s on models the explicit engine walks
   instantly — smokers6 at 494 concrete states, Gas `correct_change`
   at 165. The pattern: the monitor watches the *big* leaf's labels.
   Diagnose from the TSV's ns split and prescribe (policy? budget
   order? a spec §10 relaxation?). This is the refinement-economics
   corpus T-D was waiting for, found in the wild.
2. **CAC08 subjects 8 and 17** (Chiron property 8) exist only as QRE +
   Ada — no FSP in the artifact. Hand-translate from QRE, or drop them
   and state the gap? The other 30 subjects are in hand.
3. T-D needs its corpus: the families resolve in too few rounds for
   policies to diverge — see item 1 before designing a synthetic one.
4. Decide the next relaxation to spec (paper §10 order: interior
   contracts first).

## TODO — engineering

1. Candidate cleanup when touched next: the `cegar` summary could
   report per-entry rungs (name → index) for cone-of-influence
   tables.

Blockers: none. (CAC08 is translated, swept, and pinned — see report
§CAC08; next CAC08 action is theory's, item 1.)
