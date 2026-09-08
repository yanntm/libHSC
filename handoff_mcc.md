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
`oracle/<M>-{RC,RF,UB,RD}.out`), the incremental `hsc::surface::session`
(`include/hsc/surface/translate.hh`: `session s(out); s.feed(forms);`), the
atom printer `include/hsc/petri/props_to_surface.hh` + `src/petri_props.cc`
(compiles as part of `petri_import`, see `tools/CMakeLists.txt`).

## Next: `tools/hsc-pn.cc` (+ `tools/pn_solver.hh`), Phase 2 of the plan

Design in `tools/README.md` (read it first; keep it in sync). Facts needed:

* Load: PNML `SparsePetriNet<int>* net = loadXML<int>(path)`
  (`hsc/petri/parse/PTNetLoader.h`), units `hsc::petri::read_units(path)`
  (`nupn.hh`); PNET `petri::PNETIO<int>::read(path)` (`hsc/petri/io/PNETIO.h`);
  Louvain `hsc::petri::decompose(*net)` (`decompose.hh`); flat = a default
  `unit_tree{}`. Log to stderr: `petri::setLogStream(std::cerr)`
  (`hsc/petri/core/Log.h`). Copy the includes and the load code from
  `tools/hsc-mcc.cc`.
* Model text: `hsc::petri::to_surface(os, *net, units, opts)` with
  `opts.exam = examination::none`, `opts.bound` from `--bound` (default 2;
  the effective leaf domain is `[0, max(bound, max marking + 1))`, compute
  the same value in the tool for the bound search). Then
  `hsc::surface::parse(text)` (`hsc/surface/sexpr.hh`) gives the forms;
  append `parse("(reorder-force)")` when `--force`, and
  `parse("(reach R saturate)")`. First `session.feed` = that whole batch.
* Properties: `petri::loadPropertyFile<int>(file, *net, syntax)`
  (`hsc/petri/parse/PropertyFile.h`, `petri::propertySyntaxOf("auto|mcc|sexpr")`)
  gives `std::vector<petri::expr::Property>`; fields `name`, `kind`
  (`PropertyKind::Reachability|Invariant|Deadlock|Bound|CTL|Unsupported`),
  `body` (Expression), `boundHint` (-1 = none). Fold constants with
  `petri::expr::simplify(expr)` (`expr/Simplify.h`) before printing.
* One query: feed `(select Qi R ATOM) (count Qi)` where `ATOM` comes from
  `hsc::petri::query_atom(expr, net->getPnames())`; the session writes
  `Qi count N` on its stream. Use an `std::ostringstream` as the session's
  stream, read the lines after each feed, clear it (`str("")`). N is zero or
  not: Reachability TRUE iff non-zero; Invariant uses `makeNot(body)` then
  simplify, FALSE iff non-zero; Deadlock uses `hsc::petri::deadlock_atom(*net)`
  (nullopt = no deadlock = FALSE), TRUE iff non-zero. Root constants
  (`expr.isConstant()`): answer directly with `TECHNIQUES TOPOLOGICAL TRIVIAL`.
* Bound: binary search on k for the largest k with `(select Q R (>= FORM k))`
  non-empty, `hsc::petri::at_least(p.body.atom, k, pnames)`; k in
  `[-(B-1)*sum(|c|, c<0), (B-1)*sum(|c|, c>0)]`, B the effective bound;
  check `boundHint` first when >= 0. Print `FORMULA name k TECHNIQUES ...`.
* CTL / Unsupported: print `UNKNOWN name` only when `--printUnknown`.
* Output lines (stdout, `std::endl` to flush): `FORMULA <name> TRUE|FALSE
  TECHNIQUES DECISION_DIAGRAMS SATURATION`; `--states`: `STATE_SPACE STATES
  <n> TECHNIQUES ...` from `(count R exact)` (line `R count <n>`),
  `STATE_SPACE MAX_TOKEN_IN_PLACE <n>` from `(max-value R)` (line `R
  max-value <n>`), `STATE_SPACE MAX_TOKEN_PER_MARKING` by the bound search on
  a LinearAtom with every place at coefficient 1, `STATE_SPACE TRANSITIONS`
  = sum over t of `(count (select Gt R guard_t) exact)` (`guard_atom(net,t)`,
  nullopt means the whole R); sum with `mpz_class`.
* Overflow: catch `hsc::overflow_error` (`hsc/util/errors.hh`) around a
  feed: print `UNKNOWN` for the rest when asked, message on stderr, exit 0.
* `--totalTime S`: `alarm(S)`; the SIGALRM handler `write(1, ...)`s
  prepared `UNKNOWN <name>\n` strings from a global index of the next open
  property, then `_exit(0)`. `--export-hsc FILE` writes the model text.
* CLI11 (`CLI::App`, `add_option`, `add_flag`, `CLI11_PARSE`): copy the
  style of `tools/hsc.cc`. Keep `hsc-pn.cc` under 300 lines: the solver
  (session + decoding + bound search) in `tools/pn_solver.hh`.

Build and check (cap every run at 15 s):

```
cmake --build build -j > tests/logs/build.log 2>&1; grep error tests/logs/build.log
./build/tools/hsc-pn -i examples/mcc/Raft-PT-02.pnml --props examples/mcc/Raft-PT-02.ReachabilityCardinality.xml
./build/tools/hsc-pn --net examples/mcc/Raft-PT-02.pnet --props examples/mcc/Raft-PT-02.ReachabilityCardinality.sexpr
diff <(grep ^FORMULA examples/mcc/oracle/Raft-PT-02-RC.out | cut -d' ' -f2,3 | sort) <(./build/tools/hsc-pn ... | grep ^FORMULA | cut -d' ' -f2,3 | sort)
```

Then extend `tools/check_samples.cmake` (`-DHSC_PN` is already passed):
for each model with fixtures, run both paths on RC, RF, UB and compare the
`FORMULA name value` pairs with `oracle/<M>-<EXAM>.out`; the deadlock column
of `expected.csv` against `--deadlock ReachabilityDeadlock`. Run
`ctest --test-dir build -R mcc_samples`. Commit, push (`git push origin
master`), watch the CI (`doc/ci.md`), then add `hsc-pn` to the binary list
in `build_hsc.sh` and to `MCC-drivers/hsc/install.sh`.

## After that (Phases 3 and 4, ITS-Tools; see HSC_PLAN.md sections 3, 4)

Work in `~/git/ITStools`: new plugin `interop/fr.lip6.move.gal.interop`
(move `KERSFormatIO`, `PNETFormatIO`, `SexprPropertyPrinter` out of
`petrispot/fr.lip6.move.petrispot.runner`, which then requires it), plugins
`hsc/fr.lip6.hsc.binaries` (copy `petrispot/fr.lip6.petrispot.binaries`,
`pom.xml` `<get>` from `https://github.com/yanntm/libHSC/raw/HSC-Linux/hsc-pn`)
and `hsc/fr.lip6.move.hsc.runner` (`HscRunner` modelled on
`PetriSpotWalker`); register in `fr.lip6.move.gal.parent/pom.xml`,
`pnmcc/fr.lip6.move.gal.feature.pnmcc/feature.xml`, the application's
`MANIFEST.MF`. Build: `cd fr.lip6.move.gal.parent && mvn -o install
-DskipTests > /data/ythierry/itstools-mvn.log 2>&1` (PetriSpot
`docs/BUILD.md`). Add only the files you changed (the tree has the user's
own edits). Push is the user's unless allowed in session.

## Rules of the road

Commit per logical change with `git commit -F -` and a heredoc; never
rewrite history; no agents; runs capped at 15 s and logged under
`tests/logs/`; the vendored tree is never edited here (change PetriSpot,
then `include/hsc/petri/vendor.sh`). OSX CI is not a concern this session.
