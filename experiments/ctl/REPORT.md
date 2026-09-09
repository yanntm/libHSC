# CTL thread — session report

Narrative of the unsupervised session (started after the M4 commit
2e11ecc), for the user to read at its end. Current state and next actions
stay in `handoff_ctl.md`; the numbers in `README.md` here.

## Summary (read this first)

* The CTL checker is complete for the MCC examinations and **never wrong**
  on the corpus tried: 0 wrong verdicts on every benchmark run of the 294
  P/T instances below 10^7 states (about 6100 answered per run), and 96/96
  on the three sample nets against the 2026 oracle.
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
* Not done: no cluster run (as instructed); ITS-Tools and MCC-drivers
  changes are committed but not pushed; libHSC is pushed so the CI
  republishes `hsc-pn`.

Numbers for m5b/m5c/m5d are filled in at the end of the session below.

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
