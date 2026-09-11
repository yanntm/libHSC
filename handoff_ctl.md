# Handoff — CTL in libHSC

Current state only, next action first. Design: `research_notes/ctl_directions.md`
(the reference engine read against ours, the milestones). Rules of the
checker: `include/hsc/ctl/algorithm.md`; the core additions it needs:
`include/hsc/core/algorithm.md` §8 (gfp) and §9 (inverse with context).

## Engineering — next

1. **The portfolio read** (`experiments/ctl/README.md`): m4 → m5d is
   6101 → 6331 answered over 588 runs (294 instances below 10^7 states,
   60 s); the shapes: Louvain + FORCE 7267, NUPN + FORCE 7067, best of
   the three 7897 and 471 complete files; 0 wrong throughout. The 10^7–10^9
   list at 120 s under Louvain + FORCE answers 2105 over 258 runs, 0 wrong,
   72 runs with no answer — those are counter nets where the Louvain shape
   never finishes `R`. Running or queued: `louvain` alone on the small
   corpus (m5g); then the throughput build `hsc-pn-m6a` on the small
   corpus (default shape, against m5d); then the big corpus at `nupn`.
2. **Throughput of one image on a wide integer domain** is the lever on
   counter nets: every backward closure costs about one `R`, the `gfp`
   runs 2–9 rounds only. Done, measured on TwoPhaseLocking-PT-nC00100vN's
   `R` (2.2 → 1.6 s, 19.0 G → 11.9 G instructions): scratch buffer in
   int_set set ops, `injective(term)` on the support contract so the image
   path skips the sieve, an index by sub in the accumulator. Next on the
   profile: the hash combiner run over every arc at node interning (13.5%
   of instructions — a design point, yours), the sieve inside `do_join`
   (inherent), 12.5 M `operator new` calls (an accumulator capacity hint
   is under A/B by instruction count). Wall time on this machine drifts
   with load: decide on callgrind counts, not on `time`.
3. **Backward closures with protected events**: `compose(within(R), p)` is
   an unfusable straddler. Measured: 326 of 588 small-corpus runs invert,
   all of them protect some events, TokenRing-PT-010 all 1111 — its `AU` /
   `EU` closures cost 1.1–1.3 s each. Hulls now take the raw converses
   (no protection owed); the lfp closures still need it. Queued:
   `HSC_CTL_PROTECT=never` on the small corpus. The fix is Theory's
   constrained saturation (below).
4. **The campaign** (M6) ran on the whole corpus and is collected:
   `hsc/20260910`, 1681 P/T instances x 2 examinations, 600 s, small% at 6
   cores, the driver's four-shape portfolio — **0 wrong on 19591 answers**,
   9703 / 9888 answered, 456 / 459 complete files
   (`experiments/ctl/REPORT.md`, the last section). What stops a run is the
   budget (13155 of 13380 CTLC missed values are at the wall); the only error
   string in the corpus is `std::bad_alloc` in 223 / 226 logs, a portfolio
   member reaching its quarter of the 16 GB. Next on it: a single-shape run
   at the full memory, to price that quartering. The `hsc` MCC driver
   declares CTLCardinality / CTLFireability (`~/git/MCC-drivers/hsc/`,
   committed); ITS-Tools `-hsc` also runs the checker on the CTL
   examinations (`~/git/ITStools`, committed, not pushed; the product's
   `hsc-pn` comes from the libHSC CI branch `HSC-Linux`, so a libHSC push is
   what updates it).
5. Witness trees are in (`include/hsc/trace/`, `(path …)`, `(witness …)`,
   `hsc-pn --witness` for CTL and for reachability, invariant, deadlock);
   left open there: a shortest-overall end state for
   shapes other than `filter(fwdu(…))`, and a replay check of a printed
   witness on the explicit engine.

Observation points: `HSC_CTL_TRACE=1` (per-node wall time and `gfp` rounds on stderr).
Variation points: `HSC_CTL_EXIST=0`, `HSC_CTL_OTF=0`, `HSC_CTL_FWD=left`.

6. The shape thread has its own handoff: `handoff_order.md`.

## Theory — open

* A saturation schedule for `gfp` (none known; breadth-first for now).
  Measured variants: the round form wins over the frontier form (m6b, 6414
  against 6323). Untried, from libITS's LTL experience: the two-way trim
  `X ← X ∩ pred(X) ∩ succ(X)` to fixpoint — a smaller core, chains peeled
  from both ends — then `EG f = D ∪ lfp(sel_f ∘ pred)·core`, a saturating
  closure; also the acyclicity test by that core. One-way-catch-them-young's
  forward step is the identity for `EG` (every `f`-state is accepting);
  acceptance sets are LTL's business.
* Witness memory: the BFS layers are kept (as libITS does); checkpoint every
  k-th layer and recompute between if a model ever hurts.
* The constrained-closure rewrite (`ctl_directions.md` §4.4) stated as a
  rule in `saturate()`'s vocabulary, with its commutation criterion — now
  with evidence: `lfp(within(R) ∘ Σ pred_e)` is breadth-first wherever the
  events are protected, and on counter nets that is every event; the
  saturation should carry `R`'s sub-node down the recursion instead of
  meeting with the whole of `R` at the top.

## Done

* Directions note; `ctl/` docs; `core/algorithm.md` §8–§11;
  `research_notes/invert.md`; `experiments/ctl/` (bench script, record,
  session report).
* `ctl/formula.hh`, `ctl/forward.hh`, `ctl/checker.hh`: the forward form,
  backward `Sat`, existential leaves (`has_image`), on-the-fly search,
  lazy inverted events, one checker per session, deadlines.
* Core: `gfp`, `within`, `inverter`, `has_image`, `compose_at`, the
  interrupt hook; `int_set`: `choose`, `invert_local`, `term_compose`;
  differentials over 150 random models for inverse, has_image-free
  composition.
* Surface `(ctl …)`, `(expect-ctl …)`, `(gfp …)`, `(invert …)`,
  `(deadlock)`; manual §8f; `examples/models/ctl_*.hsc`.
* `trace/`: shortest paths (layers + backtrack through inverted events) and
  the witness tree of a `ctl` verdict; differential of the path against an
  explicit BFS.
* `hsc-pn` answers CTLCardinality / CTLFireability under fair shares; no-effect
  transitions kept for CTL; `examples/mcc` fixtures + oracles from
  pnmcc-models-2026 (96/96 on the three small nets); baseline benchmark:
  0 wrong verdicts on 6101 answered.
