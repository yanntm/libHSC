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

## Phase 3 (ITS-Tools plugins): files written, build not yet verified

In `~/git/ITStools` (uncommitted; add only these paths, the tree carries the
user's own edits):

* `interop/fr.lip6.move.gal.interop/` — `KERSFormatIO`, `PNETFormatIO`,
  `SexprPropertyPrinter` moved (`git mv`) from the PetriSpot runner, package
  `fr.lip6.move.gal.interop`; the PetriSpot runner's MANIFEST requires it
  and its two classes import `fr.lip6.move.gal.interop.*`.
* `hsc/fr.lip6.hsc.binaries/` — copy of the PetriSpot binaries plugin,
  `pom.xml` downloads `hsc-pn` from `HSC-Linux` / `HSC-OSX`,
  `BinaryToolsPlugin.getHscURI()`.
* `hsc/fr.lip6.move.hsc.runner/` — `HscRunner` (PNET + forms out, `FORMULA`
  lines in; `runReachability`, `runDeadlock`, `runBounds`, `run`;
  `-Dhsc.bin=<path>` outside OSGi; `HscRunner.DEBUG`).
* Registered in `fr.lip6.move.gal.parent/pom.xml` (three modules),
  `pnmcc/fr.lip6.move.gal.feature.pnmcc/feature.xml`, the application's
  `MANIFEST.MF` (`fr.lip6.move.hsc.runner`).

To verify: `cd ~/git/ITStools/fr.lip6.move.gal.parent && mvn -o install
-DskipTests > /data/ythierry/itstools-mvn-hsc.log 2>&1` (about 2 min warm;
`grep -n "ERROR\|BUILD" /data/ythierry/itstools-mvn-hsc.log | head`). The
`<get>` of `hsc-pn` needs the Linux CI to have published it (check
`git -C ~/git/libHSC ls-tree --name-only origin/HSC-Linux` after a fetch).
Then a smoke test outside OSGi is simplest through Phase 4's `-hsc` flag;
before that, commit the plugin files in ITS-Tools (one commit).

## Phase 4 (`-hsc` in the MCC application), not started

`pnmcc/fr.lip6.move.gal.application.pnmcc/src/fr/lip6/move/gal/application/Application.java`:
add a flag `-hsc` beside `-its` (`ITS = "-its"`, `doITS`); where the
reachability examinations start `ITSRunner` (through
`MultiOrderRunner.runMultiITS`), start an `IRunner` (`runner/IRunner.java`:
`configure`, `solve`, `interrupt`, `join`) wrapping `HscRunner.run` on the
reduced `ISparsePetriNet` with the open properties of `DoneProperties`
printed by `SexprPropertyPrinter.reach(...)`, a `Listener` that records
each verdict in `DoneProperties` (`EF` as the value says, `AG` negated: see
how `PetriSpotWalker.isWitness` is used in `ReachabilitySolver`). Measure
with `its-tools -pnfolder <model> -examination ReachabilityCardinality -hsc`
against `-its` (PetriSpot `docs/BUILD.md`, "Running ITS-Tools").

## Rules of the road

Commit per logical change with `git commit -F -` and a heredoc; never
rewrite history; no agents; runs capped at 15 s and logged under
`tests/logs/`; the vendored tree is never edited here (change PetriSpot,
then `include/hsc/petri/vendor.sh`). OSX CI is not a concern this session.
