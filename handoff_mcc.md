# Handoff — libHSC in the MCC / ITS-Tools chain

Current state only, next action first. Design: `~/git/PetriSpot/HSC_PLAN.md`
(sections 1 to 16). Measurements: `~/git/PetriSpot/libHSC_in_MCC.md`.
Campaign spec: `~/git/PetriSpot/HSC_EXPERIMENTS.md`. Tool: `tools/README.md`.
Cluster recipes: PetriSpot `docs/CLUSTER.md`.

## Next actions

1. **When the campaigns drain** (see below), collect and rebuild the pages:
   `bash ~/git/PetriSpot/Petri/test/mcc/collect.sh 20260908-hsc600 RD.hsc600 OS.hsc600 --pages`
   and read them against the ITS-Tools sets at the same budget. The reruns of
   CTLC/CTLF/L go into campaign `202609080313` (`collect.sh 202609080313 CTLC
   CTLF L --pages`), which brings those three back to 1954 logs each.
2. **Submit the rest at 600 s** when the queue is short:
   `TIMEOUT=600 WALLTIME=0:15:0 CORES=4 HOSTS="tall%" TAG=hsc600 BK_TOOL=hsc
   ./run_oar.sh "oracle/*-UB.out"`, then `-RC.out`, then `-RF.out` (1953
   instances each).
3. **Pick up the oracles the corpus CI republishes**: pnmcc-models-2026 now
   ships the total examination vectors (`oracleTotals.tar.gz`, overlaid by
   `install_inputs.sh`). Once its CI has run, refresh the local copy
   (`/data/ythierry/MCC26run/oracle-2026/oracle`) and the cluster's
   `MCC26/MCC-drivers/oracle` from the published `oracle.tar.gz`.
4. **The engineering thread**: ghost contributors (`GHOSTPT`, `GMULT`) so an
   arc count survives a transition being dropped, and the arc shifts of
   `HSC_PLAN.md` section 15 so it survives a fused free component. Section 16
   argues for carrying [BLD18]'s equation system instead of more scalar
   blocks; do that at the next accounting need rather than adding a fourth.

## What is running

| what | where | state |
|---|---|---|
| libHSC ReachabilityDeadlock, 600 s | `RD.hsc600` | 1232 of 1953 logs |
| libHSC OneSafe, 600 s | `OS.hsc600` | queued behind it |
| CTLC / CTLF / L reruns (58 each) | `CTLC`, `CTLF`, `L` | queued, 1896 of 1954 logs present |

Roughly 2900 jobs waiting, 64 running. The reruns replace the 58 instances per
examination that failed on the native image's closed world; today's image no
longer hits it (checked locally on `FileSystem-COL-N05I10B15`), the failed
logs were removed on both sides, and the same instances were resubmitted at
the campaign's own 1800 s.

## What is done

* **The tool.** `hsc-pn` answers RC, RF, RD, UB, StateSpace (four values) and
  OneSafe on PNML + MCC XML or PNET + s-expressions, checked against the
  oracle on both paths (`ctest -R pn_samples`). Exact counts through GMP. A
  net beyond the build's integer width is refused, not truncated.
* **The counting record.** PNET carries optional named blocks
  (`include/hsc/petri/io/PNET.md`); ITS-Tools carries them on the net behind
  `NetBlock`/`NetBlocks` and maintains them per rule; `hsc-pn` honours `TMULT`
  (arc multiplicities), `PDROP` (tokens of removed constant places) and
  `PCOEF` (a place standing for a fused free component, folded in by the
  weighted counter of `src/surface_weighted_count.cc`). Arc counts are only
  reported with evidence that the net's arcs are those of the net it came
  from.
* **`hsc-pn --reduce`.** PetriSpot's reductions in memory before the
  portfolio (`tools/README.md`): the STATESPACE record for `--states`, the
  `prepare` pipeline for properties, the invariant-set dead test in the
  loop. The vendor list now carries `reduction/` and `lp/`. Next: the
  cluster run of StateSpace with `--reduce`.
* **StateSpace through ITS-Tools.** `-hsc` beside the diagrams, `-hscBench` /
  `-hscBenchReduce` alone; all four values exact on reduced nets
  (AutonomousCar-PT-01a, Dekker-PT-010, AirplaneLD-PT-0010, BART-COL-002).
* **Campaigns.** StateSpace at 300 s, ours and an ITS-Tools baseline, both
  collected and on the pages; the read is in `libHSC_in_MCC.md`.
* **The totals oracle.** Published in pnmcc-models-2026: 11.0M QLA, 4.4M SMA
  and 2.7M UBA objects where the contest had none, merged from two campaigns
  that agreed everywhere, cross-checked three ways, with its provenance in
  `README-totals.txt`.

## Rules of the road

Commit per logical change with `git commit -F -` and a heredoc; never rewrite
history; no agents; runs capped and logged under `tests/logs/`. The vendored
tree under `include/hsc/petri/` is never edited here: change PetriSpot, then
`include/hsc/petri/vendor.sh`. Never overwrite a script on the cluster while
its own loop is running. Only add logs to a campaign folder: the analysis
build's caches assume it (`--force` after a removal).
