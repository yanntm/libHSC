# Handoff — libHSC in the MCC / ITS-Tools chain

Bootstrap for this thread: current state only, next action first. The plan
is `~/git/PetriSpot/HSC_PLAN.md` (sections 2 to 5); the protocol is
`~/git/PetriSpot/INTEROP.md`; the tool design is `tools/README.md` here.
Rewrite this file as items complete; never append narrative.

## State

Done and pushed: CI (`doc/ci.md`, branches `HSC-Linux`, `HSC-OSX`), GMP
exact count (`(count R exact)`, `(states)`), vendored PetriSpot tree as exact
copies (`include/hsc/petri/{core,parse,expr,io}`, `vendor.sh`), fixtures in
`examples/mcc/` (4 models: `<M>.<Exam>.xml`, `<M>.pnet`, `<M>.<Exam>.sexpr`,
`oracle/<M>-{RC,RF,UB,RD}.out`), the incremental `hsc::surface::session`,
the atom printer `include/hsc/petri/props_to_surface.hh`.

**`hsc-pn` exists** (`tools/hsc-pn.cc`, `tools/pn_solver.hh`, design in
`tools/README.md`) and is checked by `tests/pn_samples.sh` (ctest
`pn_samples`, `PN_TIMEOUT=5`): both input paths, RC, RF, UB, deadlock,
STATES against the oracle. Result at the first run: 30 checks right, 0
wrong, Angiogenesis-PT-05 UpperBounds and the StateSpace extras over the
15 s cap (one UB property alone takes 0.7 s, its RC set 9.3 s: a `select`
on the 17910-node, 42M-state diagram costs about 0.5 s; a performance
topic for the sweep, not a bug).

**The MCC driver runs on `hsc-pn`** (`~/git/MCC-drivers/hsc/`, committed,
not pushed): RC, RF, RD, UB, StateSpace (all four values), OneSafe; checked
by hand on Raft-PT-02 for the five examinations against the oracle (all
match). `hsc-pn` gained `--max-tokens` for OneSafe.

## Next actions, in order

1. Confirm the Linux CI published `hsc-pn` on `HSC-Linux` (`doc/ci.md`
   commands); then in `~/git/MCC-drivers`, `bash hsc/install.sh` (downloads),
   and the harness smoke check of `~/git/PetriSpot/libHSC_in_MCC.md`
   ("The driver" section: `run_test.pl` in the deploy clone). Push
   MCC-drivers only if the user allows.
2. Sweep on the cluster: `BK_TOOL=hsc ./run_oar.sh` over the reachability
   examinations (PetriSpot `docs/CLUSTER.md`), collect, compare with
   ITS-Tools on the contest tables. Report timeouts and any wrong verdict
   in `~/git/PetriSpot/libHSC_in_MCC.md` (append a dated section).
3. Performance of `select` on large diagrams (Angiogenesis-PT-05: one
   `select` about 0.5 s on 17910 nodes / 42M states): profile
   `hsc-pn --net examples/mcc/Angiogenesis-PT-05.pnet --props
   examples/mcc/Angiogenesis-PT-05.UpperBounds.sexpr`; report before
   changing the calculus.
4. Retire `hsc-mcc` and `nupn2hsc` (`hsc-pn --export-hsc` replaces the
   latter): drop them from `tools/CMakeLists.txt`, `build_hsc.sh`,
   `check_samples.cmake` (keep `pn_samples`), `doc/ci.md`, `tools/README.md`,
   `MCC-drivers/hsc/install.sh`; `examples/mcc/README.md` then describes
   the fixtures for `hsc-pn`.

## Phases 3 and 4 (ITS-Tools): built and committed, smoke test inconclusive

Committed in `~/git/ITStools` (not pushed; the user pushes): the interop
plugin (`interop/fr.lip6.move.gal.interop`: `KERSFormatIO`, `PNETFormatIO`,
`SexprPropertyPrinter` moved from the PetriSpot runner), `hsc/fr.lip6.hsc.binaries`
(downloads `hsc-pn` from `HSC-Linux` at build time, OSX commented out),
`hsc/fr.lip6.move.hsc.runner` (`HscRunner`: `runReachability`, `runDeadlock`,
`runBounds`; `-Dhsc.bin=<path>` outside OSGi; `HscRunner.DEBUG`), and in the
MCC application the `-hsc` flag: `Application.startHsc(...)` starts a
`runner/HscSolverRunner` (open INVARIANT EF/AG, DEADLOCK, BOUNDS properties of
the reduced net to `hsc-pn`, verdicts into `DoneProperties`) before each
`MultiOrderRunner.runMultiITS(...)` call. `mvn -o install -DskipTests` passes;
the product is extracted in `/data/ythierry/itstools-local/`.

Smoke test done: `its-tools -pnfolder ~/git/libHSC/build/mcc/Raft-PT-02
-examination ReachabilityCardinality -hsc -timeout 60` (log
`tests/logs/its_hsc_raft_rc.log`): all 16 verdicts match the oracle, but all
came from the structural pipeline (initial state, walks, SMT) before the
diagram stage, so `hsc-pn` was never called; the run also lasted the full
60 s. Next:

1. Find where the reachability examinations reach `startHsc` when properties
   remain open: run a harder model (e.g. `Angiogenesis-PT-05`, extract its
   folder as for Raft) with `-hsc -timeout 60` and `HscRunner.DEBUG = 1`
   (edit, rebuild `-rf :fr.lip6.move.gal.application.pnmcc`), look for
   "Running hsc-pn" in the log. If `startHsc` is not reached, move the call
   earlier: right after `reader.rebuildSpecification(doneProps)` in the
   RC/RF block (Application.java, before the SMT runner start) is a good
   place, guarded by `doHSC`.
2. Compare `-hsc` with `-its` on a few models for answered/time; then the
   harness campaign of `HSC_PLAN.md` section 4.
3. Why the 60 s: which runner held the process; unrelated to HSC unless the
   HSC thread is the one (it is not started in this log).

## Rules of the road

Commit per logical change with `git commit -F -` and a heredoc; never
rewrite history; no agents; runs capped at 15 s and logged under
`tests/logs/`; the vendored tree is never edited here (change PetriSpot,
then `include/hsc/petri/vendor.sh`). OSX CI is not a concern this session.
