# CTL thread — session report

Narrative of the unsupervised session (started after the M4 commit
2e11ecc), for the user to read at its end. Current state and next actions
stay in `handoff_ctl.md`; the numbers in `README.md` here.

## Summary (read this first)

* The CTL checker is complete for the MCC examinations and **never wrong**
  on the corpus tried: 0 wrong verdicts on every benchmark run of the 294
  P/T instances below 10^7 states (about 6100 answered per run), 0 wrong
  on the 129 instances of 10^7–10^9 states (2105 answered at 120 s), and
  96/96 on the three sample nets against the 2026 oracle.
* Built this session, each documented first: the existential image
  (`has_image`), the on-the-fly search inside constrained closures, an
  interrupt hook with deadlines and a fair-share scheduler, product
  composition fused into constrained closures, the protection variation
  point, a `trace/` package with shortest paths and CTL witness trees, and
  the `hsc-pn` / MCC-driver / ITS-Tools (`-hsc`) plumbing for the CTL
  examinations. Differentials over random models for the inverse, the
  fusion and the path search.
* Measured (`README.md` here): the evaluation discipline buys a few percent
  more answers; the bottlenecks that remain are the shape (a flat spine over
  a thousand places, cured by the driver's Louvain configuration) and the
  breadth-first `gfp` hull of `EG` on tens of millions of states.
* Not done: no cluster run (as instructed). libHSC and MCC-drivers are
  pushed (the CI republishes `hsc-pn`; the driver declares the CTL
  examinations); the ITS-Tools plug is committed in `~/git/ITStools`, not
  pushed — a product build publishes it, your call.

Final numbers over 588 runs (294 instances below 10^7 states, 60 s): the
baseline answers 6101, the engine work takes it to 6331 (+3.8%), the
Louvain + FORCE shape to 7267 (+19%), and the best of the two shapes per
run — what the driver's portfolio approximates — 7765 (+27%), 460 of 588
files complete; 0 wrong verdicts in every run (`README.md` here).

## Starting point

`hsc-pn` answers CTLCardinality / CTLFireability by the forward form; 96/96
verdicts on ShieldRVt-PT-001A, AutoFlight-PT-01a, Raft-PT-02 against the
2026 oracle; Angiogenesis-PT-05 (42.7M states) answers one formula in 15 s.

## Actions

1. **Baseline benchmark** — `run_ctl_bench.sh` over the 294 instances with
   at most 10^7 states, binary of M4, 60 s cap, 8 side by side.
2. **Where the time goes on Angiogenesis-PT-05** (42.7M states, flat
   shape). `R` alone takes 0.36 s and a state-constrained closure 0.42 s, so
   the constrained closures are not the problem. The trace of a 60 s run
   showed formula 00 answered after 9 s (the inversion of the 64 events and
   their exactness test against `R` came first: 4 of 64 protected) and
   formula 01 — `E[¬AF(…) U …]`, a closure constrained by a *temporal*
   formula, breadth-first by nature — taking the rest, so nine cheaper
   formulas were never asked.
3. **`has_image`, the existential image** (`core/algorithm.md` §10, written
   before the code): a witness subset or none, per term kind, with the fast
   cycle witness for `gfp` (a cycle below a cut or in a head is a cycle of
   the whole); `nonempty?` leaves answered existentially at their outermost
   operator; `EG` asks for a cycle witness before paying the hull;
   inverted events asked lazily. One checker per session so memos are shared
   across the sixteen formulas of a file (they were rebuilt per property).
4. **On the fly**: a goal inside a temporally constrained forward closure is
   tested on every new frontier and the search stops at the first hit.
5. **Deadlines and rounds**: an interrupt hook on the manager, consulted once
   per iteration round; `hsc-pn` runs the CTL properties in rounds of
   growing per-property budget (a 64th of the total, then fourfold), the
   cheap ones first, an interrupted property keeping its memos. A `ctl`
   form stopped by a deadline answers `TIMEOUT`.
6. **The MCC hsc driver** (`~/git/MCC-drivers/hsc/`) declares CTLCardinality
   and CTLFireability and hands the confinement to the rounds; tested on the
   Raft-PT-02 folder (16/16 by the nupn configuration). Committed there.
7. **ITS-Tools** (`~/git/ITStools`): under `-hsc`, the CTL examinations now
   also start libHSC's checker beside the diagrams and PetriSpot's walk, on
   the same reduced net and formulas (`HscRunner.runCtl`,
   `ParallelWalk.startHscCtl`, the flag set in `Application`). Built
   offline with Maven; tested on a local product. Committed, not pushed.

   Result on Raft-PT-02 CTLCardinality, local product with this build's
   `hsc-pn` copied into the `fr.lip6.hsc.binaries` plugin: the checker
   runs per property beside `its-ctl` ("hsc-pn: 1/1 properties solved in
   36 ms", "libHSC CTL check beside the decision diagrams solved 1
   properties"); `its-ctl` prints first on this small net, so the
   attribution goes to it. The product also carries the CTL-capable
   `hsc-pn` only if the libHSC CI branch `HSC-Linux` is rebuilt (the
   plugin downloads it at build time): a libHSC push is what publishes it.
8. **Angiogenesis-PT-05 CTLCardinality after 3–5**: 5 formulas in 60 s
   (02, 09, 11, 03, 08) against 1 before; the inversion still costs about
   9 s before any backward operator can answer.
9. **A tiny model that answers nothing**: SieveSingleMsgMbox-PT-d1m04
   (3262 states, 749 transitions, more than a thousand places, flat shape)
   answered 1/16 in 60 s. `HSC_CTL_TRACE=1` (the new observation point)
   shows a backward `EF` costing 1.6 s and every backward `Sat` node about
   1.5 s: with 749 inverted events and a flat spine over a thousand leaves,
   each backward closure is expensive on a set of 3262 states. Under
   investigation (next).
10. **Runs that answered nothing** in the baseline (124 of 589) are
   largely bound by the reachable set itself: BART-PT-002 takes 7 s for
   17424 states, AutonomousCar-PT-02b and BlocksWorld-PT-01 more than 15 s.
   That is the saturation engine and the shape (the MCC driver's portfolio
   of shapes exists for it), not the checker.
11. **Product composition fused** (`core/algorithm.md` §11, written
   first): `compose_at` composes two product terms componentwise down the
   shape, `int_set::term_compose` fuses two local terms into one (guards
   conjoined, the second read after the first action, actions composed),
   so a constrained closure keeps product events and saturates. Checked by
   a differential against the sequence on 150 random models (5426
   assertions with the inverse suite). No effect on the flat Sieve model
   (0–1/16 either way: the shape is the problem there), a small one on
   Angiogenesis.
12. **Protection variation point** `HSC_CTL_PROTECT=test|never|always`;
   `never` is sound at the seed (`research_notes/invert.md` §3) and passes
   the sample suite; on Angiogenesis 6/16 against 5/16 in 60 s. The trace
   shows the remaining cost is the `gfp` hull of `EG` (an `AF` under a
   restrict): breadth-first by nature, tens of rounds over 40M-state sets.
13. **Paths and witness trees** (`include/hsc/trace/`, new package, design
   first): `(path NAME FROM TO [through ATOM])` finds a shortest run by
   forward layers and a backtrack through the inverted events applied to
   one state at a time; `(witness NAME)` explains a `ctl` verdict by
   reading the answering leaf's set expression back as paths, the backward
   explanation for what the forward form left to `Sat` (EX, EU, EG with a
   deadlock or a lasso), and reports universal subformulas as exhaustive.
   `examples/models/trace_ring.hsc`: five paths and five witnesses,
   hand-checked lengths. A bug found on the way: the answering-leaf walk
   asked `nonempty` with a question id instead of its set id.
14. **m5a read** (existential leaves, on-the-fly search, fourfold rounds):
   6141 answered against 6101, 0 wrong on both, but 341 complete files
   against 357 — 60 runs gained answers, 48 lost some: the rounds re-ask a
   property up to four times and the work of an interrupted attempt is lost
   but for its memoised sub-results, which hurt files that used to complete
   just under the cap (CANConstruction-PT-005 CTLC: 16 in 51 s, then 12).
15. **Fair shares instead of rounds**: a property may take twice the
   remaining budget divided by the open ones, a second pass shares what is
   left, a last pass gives the rest to one. CANConstruction-PT-005 CTLC is
   back to 16/16; Angiogenesis keeps 5/16. Benchmarked as `m5c`.
16. Every variation point passes the sample suite (`HSC_CTL_EXIST=0`,
   `HSC_CTL_OTF=0`, `HSC_CTL_FWD=left`, `HSC_CTL_PROTECT=always|never`):
   they change cost, never verdicts.
17. **A regression found by the benchmark and fixed**: IBM703-PT-none CTLF
   answered 16/16 in 55 s with the baseline and 1–3 with every new binary.
   The variation points isolated it in three runs (`HSC_CTL_EXIST=0` gives
   16/16, `HSC_CTL_OTF=0` does not), and the trace named the node: the
   per-arc cycle witness inside `has_image(gfp)` descends into sub-hulls
   for every arc at every level and, when no component cycles on its own,
   still pays the full hull afterwards — `EG` cost 3.8 s instead of a
   fraction. The fast path is now opt-in (`HSC_CTL_SCCFAST=1`); IBM703 is
   back to 16/16, Angiogenesis keeps 5/16. Benchmarked as `m5d`.
18. **The benchmark, all four steps** (`README.md` here): 6101 → 6141 (m5a)
   → 6181 (m5b) → 6224 (m5c) → 6331 (m5d) answered over 588 runs, 0 wrong
   at every step, complete files 357 → 359, 76 runs up and 22 down against
   the baseline. The fast-cycle-witness fix is the largest single step
   (+107). The shape variant (`m5e`, Louvain + FORCE) runs last.
19. **The shape** (`m5e`, `--shape louvain --force`): 7267 answered against
   6331, 429 complete files against 359, median wall time 2 s against
   10 s; 124 runs up, 59 down — whole files lost on TwoPhaseLocking and
   the Stigmergy nets, where the flat NUPN shape wins. The best of the two
   per run is 7765 and 460 complete files. The engine work of the night is
   worth about a quarter of what the shape is worth; both compound, and
   the driver's portfolio already runs four shapes.
20. **The bigger models** (`m5e_1e7_1e9`, 129 instances of 10^7–10^9
   states, 120 s each, 4 side by side under an 8 GB cap): 258 runs, 2105
   answered, **0 wrong**, 100 complete files, 72 runs without an answer,
   peak memory 3.3 GB — nothing crashed, nothing hit the cap. The
   correctness check on the larger corpus passes; the yield halves per run
   against the small corpus.

21. **What the empty runs wait on** (the 72 of `m5e_1e7_1e9`): not the
   `gfp`. They are counter nets (TwoPhaseLocking, SwimmingPool,
   BridgeAndVehicles, TokenRing, Anderson, Peterson, FireWire, Sudoku…)
   where the Louvain shape never finishes the reachable set —
   TwoPhaseLocking-PT-nC00100vN has 8 places, 100 tokens each, 10^7
   states: `R` takes 1.3 s on the flat spine, 55k nodes, and does not
   finish in 15 s under the three-cluster Louvain shape. The driver's
   portfolio covers this with its `nupn` configurations; the record for
   the big corpus at `nupn` is queued after the portfolio read. As a
   baseline, ITS-Tools with its SDD engine did not finish that reachable
   set within the same 15 s cap (bounded, not waited on).
22. **Where the time goes on those nets** (new observation point: the
   trace prints the `gfp` rounds of every scope): an `EG` runs 2 to 9
   rounds; every backward closure — `EG`, `AG`, `AF`, `EU` — costs about
   one `R` (1.0–1.3 s on TwoPhaseLocking, 0.6–1.0 s on Angiogenesis-05);
   with 3–5 temporal operators per formula, a formula costs 5 s and the
   file 80–100 s. The lever is the throughput of one image over a wide
   integer domain, not the depth of any closure; a profile of `R` on the
   8-counter net follows.

23. **Throughput of one image on a wide integer domain** (profile of `R`
   on TwoPhaseLocking-PT-nC00100vN under callgrind, `tests/logs/ctl_probe/`):
   half the instructions were the canonicalizer's sieve run on the image
   of every node whose head a transition touches, a fifth were malloc and
   free, a tenth the linear regroup by sub. Three changes, each measured
   on that reachable set (2.2 s, three runs each) and each passing the
   130 ctest suites and the sample nets: the int_set set operations build
   into the theory's scratch buffer (2.2 → 2.0 s); an advisory
   `injective(term)` on the support contract — a guard or a shift keeps
   disjoint primes disjoint — lets the image path skip the sieve (2.0 →
   1.8 s; `core/algorithm.md` §5); the accumulator indexes its entries by
   sub past eight of them (1.8 → 1.6 s). Instructions 19.0 G → 11.9 G
   before the third change. A tried and dropped step: a direct-mapped
   memo of int_set meet/minus/join pairs — no measurable gain, the pairs
   do not repeat. Left for you: the sanctioned hash combiner is now the
   top line (13.5%), run over every arc of every interned node — a design
   choice I did not touch. Binary snapshot `hsc-pn-m6a`; its benchmark is
   queued behind the portfolio read.
24. **Portfolio read, second configuration** (`m5f`, `--shape nupn
   --force`): 7067 answered, 0 wrong, 402 complete files; 97 runs up and
   24 down against `m5d`. FORCE on the NUPN shape is worth almost as much
   as Louvain + FORCE (7267) with fewer losses; the best of the three per
   run is 7897 answered and 471 complete files. The fourth configuration
   (`louvain` alone) runs now.

## Where things stand for you

* `handoff_ctl.md` has the next actions; `experiments/ctl/README.md` the
  numbers; every mechanism is documented beside its code
  (`core/algorithm.md` §8–§11, `ctl/algorithm.md`, `trace/algorithm.md`).
* Variation points (all sound on the sample suite): `HSC_CTL_EXIST`,
  `HSC_CTL_OTF`, `HSC_CTL_FWD`, `HSC_CTL_PROTECT`, `HSC_CTL_SCCFAST`;
  observation point `HSC_CTL_TRACE=1`.
* The two open engine questions: the breadth-first `gfp` hull of `EG` on
  tens of millions of states, and backward closures with many protected
  events. The two open scheduling questions: which conjunct goes forward
  (`HSC_CTL_FWD`), and the shape portfolio for CTL.
* Nothing was deleted; every run's outputs sit under `tests/logs/`.
