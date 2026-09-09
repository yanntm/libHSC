# Ordering heuristics — the campaign

The shape (order and hierarchy of the leaves) decides more than any engine
improvement did (`../ctl/README.md`: 6331 answered with the NUPN shape,
7267 with Louvain + FORCE, 7914 for the best of four per run). This thread
studies the heuristics that produce it, on the MCC corpus, with the CTL
benchmark as the yardstick and reachability as the cheap first signal.

## The toolbox (`include/hsc/order/`)

| heuristic | knob | status |
|---|---|---|
| NUPN units (given), flat spine | `--shape nupn|flat` | baseline |
| Louvain over the co-occurrence graph | `--shape louvain` | measured: needs FORCE behind it |
| FORCE at every level of the tree | `--force` | measured: +900 answers over Louvain alone |
| the mirror of the order | `--reverse` | in the run below |
| P-flows as sign-aware cliques | `--invariants S`, `HSC_INV_WEIGHT`, `HSC_INV_CROSS` | in the run below |
| contraction of heavy-constant flows into flat units | `HSC_INV_MERGE=K` | in the run below |
| the post-mortem: `(profile R)` then a reshape | — | instrument in; rule to write |
| Sloan's ordering (LTSmin's finding) | — | not started |

## Scenarios and metrics

* Corpus: `../ctl/models_le1e7.txt` (294 instances) at 60 s, the CTL
  examinations, 8 side by side, 6 GB — `../ctl/run_ctl_bench.sh`, one TSV
  per configuration under `../ctl/results/`. The metric is answered
  formulas and complete files; 0 wrong is a precondition.
* The cheap signal: `R` under each shape (time, `(nodes R)`, `(profile R)`)
  on the probe nets — TwoPhaseLocking-PT-nC00100vN (8 counters, 100
  tokens), TokenRing-PT-010 (safe, 1111 transitions), Angiogenesis-PT-05
  (40M states, flows of support 23). Not a proxy for yield: flat
  Angiogenesis has the fastest `R` and the worst CTL yield.
* Reading a profile: nodes per level rising then falling — a belly — says
  two distant levels are correlated; bracket what lies between them, or
  bring them together.

## Scripts

* `heuristics.tsv` — the heuristics of the sweep: name, hsc-pn flags,
  environment (`SWEEP.md` §4).
* `sweep_job.sh` — one instance, one examination, every heuristic in sequence
  at a fixed budget under the 15 GB rule; one TSV line per run with the
  record of `SWEEP.md` §3, logs beside.
* `sweep_local.sh` — the sweep on this machine, N jobs side by side, over a
  model list; results under `results/<tag>.tsv`.
* `sweep_merge.sh` — builds `<tag>.tsv` from the per-(instance, exam) rows
  files the jobs write (a shared file torn under thousands of NFS appends).
* `sweep_pages.py` — the pages of `SWEEP.md` §2 over one or more TSVs, links
  to the logs and shape files beside them.
* `sweep_oar.sh` — the same as one OAR job per (instance, examination), run
  from the deployed folder on the head node (`PetriSpot/docs/CLUSTER.md`).
* `../ctl/summarize.py` reads the CTL bench TSVs; the sweep pages
  (`SWEEP.md` §2) are the next tool to write.

## First read of `sweep1` (StateSpace rows, the sweep still running)

8475 rows: 3363 ok, 4058 `noreach` (the honest majority: the reachable set
is beyond 270 s under that shape), 162 timeouts (the 2 s margin, since
widened to 30 s), 54 + 147 kills at the node's memory cap, 31 segfaults at
1.7–3 GB on flat orders of big nets (under investigation), 3 **wrong**
values — all one bug: `MAX_TOKEN_PER_MARKING` on DoubleExponent-PT-003
answered 163 (the number of places) for 841, the maximum search being
capped at Σ coefficient × (declared bound − 1) while the leaves had gone
to 256. Fixed (`pn_solver.hh`: the upper end grows until `form ≥ hi+1` is
unreachable); the same cap bounded every UpperBounds answer. The sweep's
first return is a correctness fix. Its second: 1091 runs built `R` and never
got `MAX_TOKEN_PER_MARKING` — the binary search probed selections of a sum
over every place, the worst atom for the crossing engine. Replaced by
`(max-sum R …)`, one memoised pass over the diagram (AirplaneLD-PT-0200:
all four values in 7 s where the run had two in 270 s; DoubleExponent-PT-003
in 1.9 s); UpperBounds forms go through the same pass, exactly.

## Protocol

A heuristic enters as a flag or an environment variable, never as a new
default; it runs on the small corpus against the current best
configuration; the TSV is committed; the README row above gets its
number. The cluster comes once several strategies work on the small
examples. The driver's portfolio (`MCC-drivers/hsc/`) takes a configuration
when it is the unique best somewhere often enough to pay its quarter.

## Results

| run | configuration | answered | complete files | note |
|---|---|---|---|---|
| m5d | nupn | 6331 | 359 | `../ctl/README.md` |
| m5e | louvain + force | 7267 | 429 | |
| m5f | nupn + force | 7067 | 402 | |
| m5g | louvain | 7047 | 413 | |
| m6c_lf | louvain + force, m6c build | 7306 | 430 | the baseline of this thread's build; 16 up / 1 down vs m5e, 0 wrong |
| m6c_lfi | + `--invariants 5` | 7397 | 438 | flows as cliques: 32 up / 23 down vs m6c_lf; TokenRing-PT-010 2 → 16 and 4 → 16, SatelliteMemory-PT-X00100Y0003 0 → 16; RwMutex-PT-r0010w2000 loses 10–11 |
| m6c_lfi4 | + `HSC_INV_MERGE=4` | 7074 | 416 | heavy flows contracted: 16 up / 45 down vs m6c_lfi; every TwoPhaseLocking instance 0 → 16, every RwMutex 16 → 0 — complementary, not a default. Best of m6c_lf, lfi, lfi4: 7599 |
