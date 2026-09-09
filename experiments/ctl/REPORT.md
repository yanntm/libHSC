# CTL thread — session report

Narrative of the unsupervised session (started after the M4 commit
2e11ecc), for the user to read at its end. Current state and next actions
stay in `handoff_ctl.md`; the numbers in `README.md` here.

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
