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
   an unfusable straddler; on nets where many events are protected the
   backward closures are breadth-first. Measure how many, then decide.
4. **The campaign** (M6): the `hsc` MCC driver declares CTLCardinality /
   CTLFireability (`~/git/MCC-drivers/hsc/`, committed); ITS-Tools `-hsc`
   also runs the checker on the CTL examinations (`~/git/ITStools`,
   committed, not pushed; the product's `hsc-pn` comes from the libHSC CI
   branch `HSC-Linux`, so a libHSC push is what updates it).
5. Witness trees are in (`include/hsc/trace/`, `(path …)`, `(witness …)`,
   `hsc-pn --witness`); left open there: a shortest-overall end state for
   shapes other than `filter(fwdu(…))`, and a replay check of a printed
   witness on the explicit engine.

Observation points: `HSC_CTL_TRACE=1` (per-node wall time and `gfp` rounds on stderr).
Variation points: `HSC_CTL_EXIST=0`, `HSC_CTL_OTF=0`, `HSC_CTL_FWD=left`.

## Theory — open

* A saturation schedule for `gfp` (none known; breadth-first for now).
* The constrained-closure rewrite (`ctl_directions.md` §4.4) stated as a
  rule in `saturate()`'s vocabulary, with its commutation criterion.

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
