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

## Phase 3 (ITS-Tools plugins): written, Maven build passes; Phase 4 written, build pending

In `~/git/ITStools` (uncommitted; the tree also carries the user's own edits
to `ahg/ dve/ promela/ xta/` manifests: never add those). To commit, exactly:

```
cd ~/git/ITStools
git add interop/fr.lip6.move.gal.interop/{META-INF,README.md,build.properties,pom.xml,src} \
  hsc/fr.lip6.hsc.binaries/{META-INF,build.properties,pom.xml,src} \
  hsc/fr.lip6.move.hsc.runner/{META-INF,README.md,build.properties,pom.xml,src} \
  petrispot/fr.lip6.move.petrispot.runner fr.lip6.move.gal.parent/pom.xml \
  pnmcc/fr.lip6.move.gal.feature.pnmcc/feature.xml \
  pnmcc/fr.lip6.move.gal.application.pnmcc/META-INF/MANIFEST.MF \
  pnmcc/fr.lip6.move.gal.application.pnmcc/src/fr/lip6/move/gal/application/Application.java \
  pnmcc/fr.lip6.move.gal.application.pnmcc/src/fr/lip6/move/gal/application/runner/HscSolverRunner.java
git status --short | grep "^[AMR]"    # check: no bin/, no target/, none of the user's manifests
git commit -F - <<'MSG'
libHSC as a companion: interop plugin (KERS, PNET, sexpr printer shared with PetriSpot), fr.lip6.hsc.binaries, fr.lip6.move.hsc.runner (HscRunner), -hsc flag starting HscSolverRunner beside the decision diagrams
MSG
```

What the pieces are: `interop/fr.lip6.move.gal.interop` (the three format
classes moved from the PetriSpot runner, package `fr.lip6.move.gal.interop`);
`hsc/fr.lip6.hsc.binaries` (downloads `hsc-pn` from `HSC-Linux` at build
time; OSX commented out); `hsc/fr.lip6.move.hsc.runner` (`HscRunner`:
`runReachability`, `runDeadlock`, `runBounds`, `-Dhsc.bin=<path>` outside
OSGi, `HscRunner.DEBUG`); in the application, `runner/HscSolverRunner`
(an `IRunner`: open INVARIANT (EF/AG), DEADLOCK, BOUNDS properties of the
reduced net to `HscRunner`, verdicts into `DoneProperties`, `killAll` when
finished) started by `Application.startHsc(...)` before each
`MultiOrderRunner.runMultiITS(...)` call when `-hsc` is given.

Build: `cd ~/git/ITStools/fr.lip6.move.gal.parent && mvn -o install
-DskipTests -rf :fr.lip6.move.gal.application.pnmcc >
/data/ythierry/itstools-mvn-hsc3.log 2>&1` (the plugins before it already
built: `/data/ythierry/itstools-mvn-hsc2.log`). Then the smoke test of
PetriSpot `docs/BUILD.md` ("ITS-Tools", "Running ITS-Tools on a model
folder") with `-hsc` instead of `-its` on `~/git/libHSC/build/mcc/Raft-PT-02`
(extracted contest folder; `tar xzf
~/git/pnmcc-models-2026/website/INPUTS/Raft-PT-02.tgz -C build/mcc` if gone),
`-examination ReachabilityCardinality -timeout 60`; expect 16 `FORMULA` lines
with `HSC` in their techniques matching `examples/mcc/oracle/Raft-PT-02-RC.out`.
Then the same with `-its -hsc` together (both engines), and
ReachabilityDeadlock, UpperBounds. Then the harness campaign of HSC_PLAN.md
section 4.

## Rules of the road

Commit per logical change with `git commit -F -` and a heredoc; never
rewrite history; no agents; runs capped at 15 s and logged under
`tests/logs/`; the vendored tree is never edited here (change PetriSpot,
then `include/hsc/petri/vendor.sh`). OSX CI is not a concern this session.
