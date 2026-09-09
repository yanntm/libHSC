# `examples/mcc/` — a small MCC test set

Eight P/T nets from the Model Checking Contest corpus (`pnmcc-models-2025`
and `-2026`, the `PT` instances), chosen small and across families, with the
contest's oracle verdicts. Two tests read them:

* `mcc_samples` (`tools/check_samples.cmake`): `hsc-mcc` on every
  `<Model>.pnml` for StateSpace and OneSafe against `expected.csv`.
* `pn_samples` (`tests/pn_samples.sh`): `hsc-pn` on the four models that
  carry property fixtures, on both input paths, for ReachabilityCardinality,
  ReachabilityFireability, UpperBounds, CTLCardinality, CTLFireability,
  the deadlock and the state count, against `oracle/`. A run over the cap (`PN_TIMEOUT`, 5 s under ctest) is
  reported, only a wrong verdict fails.

| model | states | one-safe | deadlock | property fixtures |
|---|---|---|---|---|
| Sudoku-PT-AN01 | 2 | TRUE | TRUE | |
| ShieldRVt-PT-001A | 33 | TRUE | FALSE | yes |
| ERK-PT-000001 | 13 | TRUE | FALSE | |
| AutoFlight-PT-01a | 253 | TRUE | TRUE | yes |
| Raft-PT-02 | 7381 | TRUE | FALSE | yes |
| CircadianClock-PT-000001 | 128 | TRUE | FALSE | |
| Angiogenesis-PT-01 | 110 | TRUE | TRUE | |
| Angiogenesis-PT-05 | 42734935 | FALSE | TRUE | yes |

`Angiogenesis-PT-05` is the adversarial one: no unit tree (flat shape), not
one-safe, 42.7M states; its `select`s cost about 0.5 s each and its
UpperBounds and CTL sets run over the cap.

## Files

* `<Model>.pnml` — the contest model; `expected.csv` — states, one-safe,
  deadlock from the oracle (`# model,states,onesafe,deadlock`).
* `<Model>.<Examination>.xml` — the contest property files (RC, RF, UB,
  CTLC, CTLF), from `~/git/pnmcc-models-2026/website/INPUTS`, the only
  source of models, formulas and oracles here.
* `<Model>.pnet`, `<Model>.<Examination>.sexpr` — the same net and properties
  in the tool-to-tool formats of PetriSpot's `INTEROP.md`, written by
  `petri64 -i M.pnml --exportNet M.pnet` and
  `petri64 -i M.pnml --props X.xml --printProps=sexpr-index | grep '^('`.
* `oracle/<Model>-{RC,RF,UB,RD,CTLC,CTLF}.out` — the contest oracle lines
  (`pnmcc-models-2026`, `website/oracle.tar.gz`).

To add a model: drop `<Name>.pnml`, a row in `expected.csv`; for the
property paths, the three `.xml`, the `.pnet`, the three `.sexpr` and the
four oracle files.

Run by hand:

```
hsc-pn -i examples/mcc/Raft-PT-02.pnml --props examples/mcc/Raft-PT-02.ReachabilityCardinality.xml
hsc-pn --net examples/mcc/Raft-PT-02.pnet --props examples/mcc/Raft-PT-02.ReachabilityCardinality.sexpr
hsc-pn -i examples/mcc/Raft-PT-02.pnml --states --deadlock ReachabilityDeadlock
bash tests/pn_samples.sh
```
