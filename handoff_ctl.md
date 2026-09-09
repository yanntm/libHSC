# Handoff — CTL in libHSC

Current state only, next action first. Design: `research_notes/ctl_directions.md`
(the reference engine read against ours, the milestones). Rules of the
checker: `include/hsc/ctl/algorithm.md`; the core additions it needs:
`include/hsc/core/algorithm.md` §8 (gfp) and §9 (inverse with context).

## Engineering — next

1. **Read the benchmark** (`experiments/ctl/`): baseline `m4`, then `m5a`
   (existential leaves, on-the-fly search, rounds) and `m5b` (product
   composition fused), all on the 294 instances below 10^7 states at 60 s;
   `summarize.py a.tsv b.tsv` compares. Then the same with
   `-e "--shape louvain --force"`: on SieveSingleMsgMbox-PT-d1m04 (1295
   places, flat) the default shape answers 1/16 and Louvain+FORCE 16/16 —
   the shape, not the checker, decides many small instances.
2. **The inversion cost** on big `R` (Angiogenesis-PT-05: 9 s for 64 events,
   the exactness test applying each inverse to `R`): measure, then either a
   cheaper test or a lazier protection.
3. **Backward closures with protected events**: `compose(within(R), p)` is
   an unfusable straddler; on nets where many events are protected the
   backward closures are breadth-first. Measure how many, then decide.
4. **The campaign** (M6): the `hsc` MCC driver declares CTLCardinality /
   CTLFireability (`~/git/MCC-drivers/hsc/`, committed); ITS-Tools `-hsc`
   also runs the checker on the CTL examinations (`~/git/ITStools`,
   committed, not pushed; the product's `hsc-pn` comes from the libHSC CI
   branch `HSC-Linux`, so a libHSC push is what updates it).
5. Witness trees are in (`include/hsc/trace/`, `(path …)`, `(witness …)`);
   left open there: a shortest-overall end state for shapes other than
   `filter(fwdu(…))`, and `hsc-pn` printing a witness on request.

Observation points: `HSC_CTL_TRACE=1` (per-node wall time on stderr).
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
