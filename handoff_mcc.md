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

## Campaign E1 (StateSpace): draining, first read done

Submitted 2026-09-08 evening: `TIMEOUT=300 WALLTIME=0:10:0 CORES=4
HOSTS="tall%" BK_TOOL=hsc ./run_oar.sh "oracle/*-SS.out"`, 1391 instances,
binaries from `HSC-Linux`. Warmup on `AirplaneLD-PT-0010` was green on all
six examinations first. Collected as it drains into
`/data/ythierry/MCC26run/20260908-hsc/` and on the pages as the set
`libHSC 20260908 SS`:

```
bash ~/git/PetriSpot/Petri/test/mcc/collect.sh 20260908-hsc SS --pages
ssh cluster.lip6.fr 'ls MCC26/MCC-drivers/SS/OAR.*.stdout | wc -l; oarstat -u | grep -c " ythierry "'
```

First read at 163 logs, with the findings, is
`~/git/PetriSpot/libHSC_in_MCC.md` (2026-09-08 section): 309 values right,
3 wrong (all TRANSITIONS on coloured instances, the unfolder's fused
bindings, not the engine), 41 runs at the 300 s wall, median 60 s.

**Waiting in the queue**: one ITS-Tools warmup job for the 300 s SS
baseline, submitted with `TAG=itstools` so it lands in `SS.itstools/` (the
new `run_oar.sh` option; two tools, one examination, no mixing). When its
log appears and looks right, submit the baseline:

```
ssh cluster.lip6.fr 'cd MCC26/MCC-drivers && TIMEOUT=300 WALLTIME=0:10:0 CORES=4 \
  HOSTS="tall%" TAG=itstools BK_TOOL=itstools ./run_oar.sh "oracle/*-SS.out"'
bash ~/git/PetriSpot/Petri/test/mcc/collect.sh 20260908-its SS.itstools --pages
```

The ITS-Tools image in the deploy tree and on the cluster was refreshed to
the product `202609081701` (it carries the `hsc-pn` of `HSC-Linux` and the
new `-hsc` / `-hscBench` flags); `its-tools-native` is present, so the
launcher is the native image, `tall%` only.

**To deploy once the campaign has drained** (never touch a tool folder
under a running campaign):

```
cd /data/ythierry/MCC26deploy/MCC-drivers && git pull && (cd hsc && ./install.sh)
rsync -rlptD --no-g --chmod=Dg+s --delete /data/ythierry/MCC26deploy/MCC-drivers/hsc/ cluster.lip6.fr:MCC26/MCC-drivers/hsc/
```

It brings the driver's coloured guard (no TRANSITIONS on an unfolded net)
and the `hsc-pn` that reports a bad net cleanly instead of dumping core.
Also: never overwrite `run_oar.sh` while its submission loop runs (it
truncated this campaign's submission; nothing was lost, `docs/CLUSTER.md`
now says so).

## Next actions after E1

## Next actions after E1

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
