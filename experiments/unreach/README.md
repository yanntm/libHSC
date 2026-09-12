# `experiments/unreach/` — impossibility by over-approximation

The campaign of the unreachability engine (`include/hsc/linear/`,
`research_notes/unreach_report.md`): how many reachability formulas of the
MCC an over-approximation `S ⊇ R` refutes, without the reachable set, and
against what.

| file | what |
|---|---|
| `approx_sweep.sh TAG LIST [PAR]` | RC and RF of every model of LIST, three engines: `hsc` (`hsc-pn --approx 5 --approx-only`), `hscu` (the same with the NUPN unit constraints), `lp` (PetriSpot `petri64 --lp --lpTime 5`), `hscb` (`hscu` with `--approx-back 50 --approx-back-time 2`, the backward search inside S); `ENGINES` selects them; `CAP` seconds each (60) and `MEMKB` kB of virtual memory each (6 GB), parallelism 4 by default; outputs and a scored `rows.tsv` under `tests/logs/unreach/TAG/` |
| `score.py ORACLE RUN…` | FORMULA lines against a contest oracle: answered, correct, wrong, unchecked |
| `models_local.txt` | the 430 P/T instances extracted under `tests/logs/mcc2026/INPUTS/` |
| `models_sample.txt` | one in five of them, the shake-out set |

`rows.tsv` columns: tag, model, exam, engine, formulas in the oracle,
answered, correct, wrong, unchecked (oracle `?`), seconds, exit code, the
names of the wrong ones. A wrong verdict is a soundness bug, never a
statistic. Oracles: `/data/ythierry/MCC26deploy/MCC-drivers/oracle/<model>-<EXAM>.out`.

## Results

| file | campaign |
|---|---|
| `results/full2_rows.tsv` | the full local corpus (430 models, RC+RF), `hscb` and `lp`, binary a8a5dff — report §2.12 |
| `results/full1_partial_rows.tsv` | the first full pass, killed at 1203 rows, binary 1d53acc (4 wrong verdicts of the class fixed in a8a5dff) — §2.8 |
| `results/sample_rows.tsv`, `results/sampleb_rows.tsv` | the one-in-five sample, `hsc`/`hscu`/`lp` and `hscb`, binary c684504 — §2.4, §2.5 |

Headline (full corpus, 13 760 formulas, 60 s and 6 GB a run): `hscb` 6796
answered, 0 wrong; `lp` 3363 answered, 0 wrong; `hscb` ahead on 541
instances, `lp` on 244.

HSC benchmark invocations use `-v` without `-q`; complete diagnostics, including
reduction summaries and counting records, are retained in each run’s stderr file.
