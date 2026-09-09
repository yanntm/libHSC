# ctl — experiment record

Measured results of the CTL checker (`include/hsc/ctl/`, the `hsc-pn` tool)
on the Model Checking Contest 2026 corpus, locally. Inputs and oracles come
from `~/git/pnmcc-models-2026/website` **only** (`INPUTS/<instance>.tgz`,
`oracle.tar.gz`); earlier editions carry other formulas under the same ids.
Mechanisms are documented beside the code (`ctl/algorithm.md`,
`core/algorithm.md` §8–§10) and the design in
`research_notes/ctl_directions.md`, `research_notes/invert.md`; this folder
is the data record. `REPORT.md` is the narrative of the unsupervised session
that produced it.

## How

`run_ctl_bench.sh` (see its header): one `hsc-pn` process per (instance,
examination) under `timeout` and `ulimit -v`, the tool's own `--totalTime`
one second under the cap so it prints `UNKNOWN` for what it left open,
verdicts compared with the oracle, one TSV line per run under `results/`.
`models_le1e7.txt` lists the 294 P/T instances with a known state count of
at most 10^7 and a CTL oracle (the `-SS.out` counts of the oracle archive);
`models_1e7_1e9.txt` the 129 between 10^7 and 10^9, for longer caps.
Runs, logs and extracted inputs live under `tests/logs/mcc2026/` (not
tracked).

Machine: the development workstation (20 cores, 64 GB), 8 runs side by side,
60 s and 6 GB per run.

## Results

| file | binary | what |
|---|---|---|
| `results/m4_le1e7.tsv` | libHSC 2e11ecc (M4: forward form, gfp, inverse; no existential evaluation) | baseline |
| `results/m5a_le1e7.tsv` | 3cae835 minus fusion (existential leaves, on-the-fly search, deadline rounds, one checker per session) | the evaluation discipline |
| `results/m5b_le1e7.tsv` | 3cae835 (product composition fused into the constrained closures) | fusion; its last third ran beside m5a's tail |
| `results/m5c_le1e7.tsv` | f78cf27 (fair-share scheduler instead of rounds) | the scheduler |
| `results/m5d_le1e7.tsv` | the fast cycle witness of `has_image(gfp)` turned off (opt-in) | the fix of the IBM703 regression |
| `results/m5e_le1e7.tsv` | the same with `--shape louvain --force` | the shape (one configuration of the driver's portfolio) |
| `results/m5f_le1e7.tsv` | the same with `--shape nupn --force` | the second configuration of the portfolio |
| `results/m5g_le1e7.tsv` | the same with `--shape louvain` | the last configuration of the portfolio |
| `results/m6b_le1e7.tsv` | 44fd8d8 (throughput work: scratch buffer, injective hint, indexed accumulator, capacity hint; hull by frontier) | default shape, against m5d |
| `results/m6b_rounds_le1e7.tsv` | the same with `HSC_CTL_GFP=rounds` | the hull form A/B |

### m4 baseline

588 runs (294 instances × CTLC, CTLF): **6101 answered, 0 wrong**, 3259
open; 357 files complete; median wall 10 s, 90th percentile at the cap.
Of the 231 incomplete runs, 124 answered nothing — largely bound by the
reachable set itself on the default shape (BART-PT-002: 7 s for 17424
states; SieveSingleMsgMbox-PT-d1m04, 1295 places flat: `R` in 6 s, 1/16 in
60 s, while `--shape louvain --force` answers 16/16 in 21 s).

### m5a to m5e

| run | answered | wrong | open | complete files | vs previous (runs up / down) |
|---|---|---|---|---|---|
| m4 | 6101 | 0 | 3259 | 357 | — |
| m5a | 6141 | 0 | 3267 | 341 | 60 / 48 vs m4 |
| m5b | 6181 | 0 | 3227 | 344 | 28 / 12 vs m5a; 63 / 45 vs m4 |
| m5c | 6224 | 0 | 3184 | 348 | 39 / 25 vs m5b; 68 / 33 vs m4 |
| m5d | 6331 | 0 | 3077 | 359 | 36 / 13 vs m5c; 76 / 22 vs m4 |
| m5e (Louvain + FORCE) | 7267 | 0 | 2141 | 429 | 124 / 59 vs m5d |
| m5f (nupn + FORCE) | 7067 | 0 | 2341 | 402 | 97 / 24 vs m5d |
| m5g (Louvain) | 7047 | 0 | 2361 | 413 | 116 / 58 vs m5d |
| best of m5d, m5e, m5f, m5g per run | 7914 | 0 | — | 473 | the driver's portfolio, all four configurations |

| m6b, frontier hull | 6323 | 0 | 3085 | 360 | 27 / 23 vs m5d |
| m6b, round hull | 6414 | 0 | 2994 | 370 | 36 / 3 vs m5d; 31 / 10 frontier vs rounds |

Unique best configuration per run (the others strictly lower): NUPN + FORCE
27 runs, Louvain + FORCE 25, NUPN 15, Louvain alone 4 — every configuration
earns its place, Louvain alone the least.

`results/m5e_1e7_1e9.tsv`: the 129 instances between 10^7 and 10^9 states,
120 s, Louvain + FORCE, 4 side by side with 8 GB each — the correctness
check on bigger models. 258 runs (one instance has no fireability file):
2105 answered, 0 wrong, 2023 open, 100 complete files, 72 runs with no
answer at all; peak memory 3.3 GB, every run uses its whole budget (the
fair shares spend what is left on the open formulas). Half the answers of
the small corpus per run, at twice the budget — the cost of a decade of
states is a factor of four here, and the open engine question (the `gfp`
hull of `EG`) is what the zero-answer runs mostly wait on.

The evaluation discipline (m5a), the fusion (m5b) and the fair-share
scheduler (m5c) each buy about forty answers over 588 runs (6101 → 6224,
+2%); the rounds of m5a cost complete files, which the fair shares give
back. One file (IBM703-PT-none CTLF, 16 → 1–3) exposed the fast cycle
witness as a loss — turning it off (m5d) is the largest single gain,
+107 answers and 11 more complete files. Over the four steps: 6101 → 6331
answered (+3.8%), 357 → 359 complete files, 0 wrong throughout. The six
runs ended by the outer `timeout` in m5c/m5d are the largest nets of the
list (ServersAndClients-PT-200320 / -400160, RERS2020-PT-pb101), whose
parsing and emission precede the tool's alarm.

**The shape.** The same binary with `--shape louvain --force` (m5e) answers
7267 (+15% over the default shape, +19% over the baseline), completes 429
files and halves the median wall time (10 s → 2 s); it loses on 59 runs,
the largest losses whole files (TwoPhaseLocking-PT-nC00050, the Stigmergy
nets), so the two configurations complement each other: the best of the
two per run is 7765 answered and 460 complete files — the MCC driver's
portfolio runs four configurations and keeps the first complete one, so its
score should sit between. No configuration ever answered wrongly.

Further rows are appended as the runs complete.
