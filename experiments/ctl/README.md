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
at most 10^7 and a CTL oracle (the `-SS.out` counts of the oracle archive).
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
| `results/m5d_le1e7.tsv` | f78cf27 with `--shape louvain --force` | the shape (one configuration of the driver's portfolio) |

### m4 baseline

588 runs (294 instances × CTLC, CTLF): **6101 answered, 0 wrong**, 3259
open; 357 files complete; median wall 10 s, 90th percentile at the cap.
Of the 231 incomplete runs, 124 answered nothing — largely bound by the
reachable set itself on the default shape (BART-PT-002: 7 s for 17424
states; SieveSingleMsgMbox-PT-d1m04, 1295 places flat: `R` in 6 s, 1/16 in
60 s, while `--shape louvain --force` answers 16/16 in 21 s).

Further rows are appended as the runs complete.
