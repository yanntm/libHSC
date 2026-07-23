# handoff — parametric surface

Spec: `research_notes/hsc_parametric.md` — Part I (M1–M3) done and green,
Part II (§6–11, M4–M7) is the current contract: **symmetric encodings, the
declared route**. Report: `research_notes/hsc_parametric_report.md`.

## Engineering (next action first)

1. **M4 — symbolic pipeline + declared fold** (spec §6–7): certificate
   checker in the expander (emit `family`, unexpanded), symbolic
   arrays/shape/init (no cell atoms, bignum param fold), translator builds
   the recurrence over the sort DAG. Gate: code equality of declared vs
   enumerated+`sum_at` on philo/mutex at N ∈ {4,8,16,64}; build cost flat
   in N. A code-inequality with agreeing reach is a finding — explain it
   before proceeding.
2. **M5 — schedule consumes the fold** (spec §8): partition reads the
   head-folded form without flattening; instrument distinct fixpoint
   closure keys — the O(log N) claim is that counter. May localize the
   standing superlinear translator/engine cost (×4 N → ×12 time at
   N=16384) for free; that item stands until named.
3. **M6 — cardinality** (spec §9): exact bignum + log-form. Verify the
   philo recurrence a_N = 2a_{N−1}+a_{N−2} (a_1=2, a_2=6) at N=3..8 on
   both engines first — it is Theory-derived from two report points.
4. **M7 — the 2^100 headline** (spec §10): nodes/op-terms/closures/time
   plus log-count vs oracle, and one folded invariant queried at scale.
5. On demand: `-DN=` CLI override of `param`.

## Theory

1. Respond to M4–M7 findings as they land (code inequality at gate 1,
   closure-sharing degradation at gate M5.1 are the anticipated ones).
2. On M7 green: integrate into hsc_core5 §8 and close Open 6's working
   half; write up the common-factor generalization of Prop 4.5
   (`Σ_i (g ⊗ w_i) = g ⊗ R`, spec §7).
3. Later, unblocked by Part II: orbit tags / quotient reduction — the
   fold shrinks the representation, the quotient would shrink the space.

## Blockers

None.
