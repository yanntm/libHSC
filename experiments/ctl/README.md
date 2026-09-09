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

### m4 baseline

588 runs (294 instances × CTLC, CTLF): **6101 answered, 0 wrong**, 3259
open; 357 files complete; median wall 10 s, 90th percentile at the cap.
Of the 231 incomplete runs, 124 answered nothing — largely bound by the
reachable set itself on the default shape (BART-PT-002: 7 s for 17424
states; SieveSingleMsgMbox-PT-d1m04, 1295 places flat: `R` in 6 s, 1/16 in
60 s, while `--shape louvain --force` answers 16/16 in 21 s).

### m5a, m5b, m5c

| run | answered | wrong | open | complete files | vs previous (runs up / down) |
|---|---|---|---|---|---|
| m4 | 6101 | 0 | 3259 | 357 | — |
| m5a | 6141 | 0 | 3267 | 341 | 60 / 48 vs m4 |
| m5b | 6181 | 0 | 3227 | 344 | 28 / 12 vs m5a; 63 / 45 vs m4 |
| m5c | 6224 | 0 | 3184 | 348 | 39 / 25 vs m5b; 68 / 33 vs m4 |

The evaluation discipline (m5a), the fusion (m5b) and the fair-share
scheduler (m5c) each buy about forty answers over 588 runs (6101 → 6224,
+2%); the rounds of m5a cost complete files, which the fair shares give
back. One file (IBM703-PT-none CTLF, 16 → 1–3) exposed the fast cycle
witness as a loss — addressed in m5d. Six m5c runs ended by the outer
`timeout` rather than the tool's own alarm (to look at: a phase the alarm
does not reach).

Further rows are appended as the runs complete.
