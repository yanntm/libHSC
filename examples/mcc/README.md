# `samples/` — test corpora

`divine/` holds the BEEM (DVE) benchmark and its generated `.hsc` forms —
see its own README, citation included. The rest of this folder is the NUPN
set below.

## A small NUPN test set

Six safe P/T nets with NUPN unit trees, from the Model Checking Contest corpus
(`pnmcc-models-2025`, the `PT` instances), chosen small and across families. They
drive the `mcc_samples` sanity test: `hsc-mcc` imports each and answers the three
fixed examinations, checked against the MCC 2025 oracle.

| model | states | one-safe | deadlock |
|---|---|---|---|
| Sudoku-PT-AN01 | 2 | TRUE | TRUE |
| ShieldRVt-PT-001A | 33 | TRUE | FALSE |
| ERK-PT-000001 | 13 | TRUE | FALSE |
| AutoFlight-PT-01a | 253 | TRUE | TRUE |
| Raft-PT-02 | 7381 | TRUE | FALSE |
| CircadianClock-PT-000001 | 128 | TRUE | FALSE |
| Angiogenesis-PT-01 | 110 | TRUE | TRUE |
| Angiogenesis-PT-05 | 42734935 | FALSE | TRUE |

`Angiogenesis-PT-05` is the adversarial one: it carries **no** unit tree (so the
importer falls back to a flat shape), it is **not** one-safe, and it has 42.7M
reachable states — all three answered correctly.

The verdicts are the oracle values (`website/oracle/MODEL-{SS,OS,RD}.out`,
`ORACLE2025`/`TEDD2023`), recorded in `expected.csv` — the single source the test
reads. To add a model: drop its `model.pnml` here as `<Name>.pnml` and add a row.

Run one by hand:

```
hsc-mcc samples/Raft-PT-02.pnml -mcc StateSpace
hsc-mcc samples/Raft-PT-02.pnml -mcc OneSafe
hsc-mcc samples/Raft-PT-02.pnml -mcc ReachabilityDeadlock
```
