# Handoff — libHSC in the MCC / ITS-Tools chain

Bootstrap for this thread: current state only, next action first. The plan
is `~/git/PetriSpot/HSC_PLAN.md` (sections 2 to 5); the protocol is
`~/git/PetriSpot/INTEROP.md`; the tool design is `tools/README.md` here.
Rewrite this file as items complete; never append narrative.

## State

Done and pushed: CI (`doc/ci.md`, branch `HSC-Linux`; OSX not maintained), GMP
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

## Phases 3 and 4 (ITS-Tools): built, committed, `-hscBench` verified

Committed in `~/git/ITStools` (not pushed; the user pushes): the interop
plugin (`interop/fr.lip6.move.gal.interop`: `KERSFormatIO`, `PNETFormatIO`,
`SexprPropertyPrinter` moved from the PetriSpot runner), `hsc/fr.lip6.hsc.binaries`
(downloads `hsc-pn` from `HSC-Linux` at build time, OSX commented out),
`hsc/fr.lip6.move.hsc.runner` (`HscRunner`: `runReachability`, `runDeadlock`,
`runBounds`; `-Dhsc.bin=<path>` outside OSGi; `-Dhsc.debug=1|2`), and in the
MCC application `runner/HscSolverRunner` (an `IRunner`: open INVARIANT EF/AG,
DEADLOCK, BOUNDS properties of the reduced net to `hsc-pn`, constant-folded
ones reported as `TOPOLOGICAL INITIAL_STATE`, verdicts into `DoneProperties`)
started two ways:

* `-hsc`: `Application.startHsc(...)` before each `MultiOrderRunner.runMultiITS`
  call, beside the decision diagrams. On Raft the structural pipeline
  settled everything first, so `hsc-pn` was not reached there (a harder
  model or an earlier call site, see below).
* `-hscBench` (`-hscBenchReduce` with the structural reductions first): the
  libHSC engine alone right after the model and properties are read, the
  `LouvainBench` idea. Measured: Raft-PT-02 RC in 1.2 s, Angiogenesis-PT-05
  RC 16/16 right in 16.7 s (`tests/logs/its_hscbench_*.log`).

Build: `cd ~/git/ITStools/fr.lip6.move.gal.parent && mvn -o install
-DskipTests -rf :fr.lip6.move.hsc.runner > /data/ythierry/itstools-mvn.log
2>&1`; product extracted with `tar xzf
ITS-commandline/fr.lip6.move.gal.itscl.product/target/products/*linux*.tar.gz
-C /data/ythierry/itstools-local`; run `its-tools -pnfolder <model folder>
-examination ReachabilityCardinality -hscBench -timeout 60`.

Next:

1. Harness: an `itstools`-style driver entry that runs the product with
   `-hscBenchReduce` on RC, RF, RD, UB (a `BK_TOOL` in `~/git/MCC-drivers`,
   copy `itstools/` conventions), then the cluster campaign of
   `HSC_PLAN.md` section 4 against `-its`; report in
   `~/git/PetriSpot/libHSC_in_MCC.md`.
2. `-hsc` placement: to make HSC part of a normal run, start it right after
   `reader.rebuildSpecification(doneProps)` in the RC/RF block of
   `Application.java` (before the SMT runner), guarded by `doHSC`.
3. StateSpace through ITS-Tools: `HscSolverRunner` handles properties only;
   `hsc-pn --states` is the driver's path (`MCC-drivers/hsc/`).

## Rules of the road

Commit per logical change with `git commit -F -` and a heredoc; never
rewrite history; no agents; runs capped at 15 s and logged under
`tests/logs/`; the vendored tree is never edited here (change PetriSpot,
then `include/hsc/petri/vendor.sh`). OSX CI is not a concern this session.
