# Handoff — CTL in libHSC

Current state only, next action first. Design: `research_notes/ctl_directions.md`
(the reference engine read against ours, the milestones). Rules of the
checker: `include/hsc/ctl/algorithm.md`; the core additions it needs:
`include/hsc/core/algorithm.md` §8 (gfp) and §9 (inverse with context).

## Engineering — next

1. **Measure** on the MCC nets, per model: events protected by `within(R)`
   (Raft-PT-02: 12 of 44), node counts of backward sets against `R`, time
   of `invert_events`, share of formulas whose forward form has a `restrict`
   leaf. Angiogenesis-PT-05 (42.7M states) answers one CTLC formula in 15 s;
   where the time goes there is the first profile to take.
2. **The campaign** (M6): CTLC / CTLF at 600 s on the cluster through
   `MCC-drivers/hsc/`, beside the ITS-Tools sets; the `.hsc600` recipe of
   `handoff_mcc.md`.
3. Then the optimisations on the produced questions: `has_image`, the
   constrained-closure rewrite (M5).

Fixtures and oracles come from `~/git/pnmcc-models-2026/website` only
(`INPUTS/<model>.tgz`, `oracle.tar.gz`); older editions carry other formulas
under the same ids.

## Theory — open

* A saturation schedule for `gfp` (none known; breadth-first for now).
* The constrained-closure rewrite (`ctl_directions.md` §4.4) stated as a
  rule in `saturate()`'s vocabulary, with its commutation criterion.

## Done

* Directions note; `ctl/` docs; `core/algorithm.md` §8–§9;
  `research_notes/invert.md`.
* `ctl/formula.hh`, `ctl/forward.hh`: DAG, NNF, existential dual, the VIS
  rules to a question tree; the and-rule sends the right conjunct forward
  (a choice to re-examine, `ctl/algorithm.md` §4).
* `op_kind::gfp`, `op_kind::within`; `core::inverter`;
  `support_algebra::invert_local` (optional); `int_set` `choose` and
  `invert_local`; differential over 150 random models.
* `ctl/checker.hh`; surface `(ctl …)`, `(expect-ctl …)`, `(gfp …)`,
  `(invert …)`, `(deadlock)`; manual §8f; `examples/models/ctl_*.hsc`.
* `hsc-pn` answers CTLCardinality / CTLFireability; no-effect transitions
  kept for CTL (self-loops are edges — the one wrong verdict before that);
  `examples/mcc` fixtures + oracles, 96/96 on the three small nets.
