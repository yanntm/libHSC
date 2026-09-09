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
