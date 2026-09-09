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

Summaries are appended below as the runs complete.
