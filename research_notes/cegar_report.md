# CEGAR v1 — engineering report

Response to `cegar_spec.md` (v1: finite instance). Status: **M0–M6
delivered**; all oracles wired; campaign data below. Reproducibility:
every number traces to `experiments/cegar/*.tsv`, produced by
`experiments/cegar/sweep.sh` against the committed tree; models are
regenerated from recorded seeds by `hsc-cegar gen`, so the TSVs and the
script are the whole artefact.

## Delivered

* Package `include/hsc/cegar/` + `src/cegar/` (own static lib
  `hsc_cegar`, isolation as specified: `util/` only).
* Tools `tools/cegar/`: `hsc-cegar` (run / mono / gen),
  `hsc-certcheck` (independent, zero shared code).
* Tests `tests/cegar/` (own binary): 18 cases / 1210 assertions, all
  green; oracles O1–O4 in-process, O5 via the sweep + a pinned
  in-sweep mutation battery (G1 drop, action redirect, dead-flip both
  ways: all rejected).
* Campaign `experiments/cegar/`: T-A/T-B, T-C, T-D.

## Findings the spec (or paper) should absorb

**F1 — the initial hypothesis must be published as chaos; anything
else is unsound until certified.** The learner's observation table,
closed at construction, already distinguishes letters by
initial-state firability; publishing that table uncertified rejects
genuine traces and is not an over-approximation. First sweep: 5/200
random models returned false `holds`, and `hsc-certcheck` rejected
all 25 certificates built on uncertified tables (obligation L) — the
independent checker caught the loop's bug, which is precisely the
architecture's claim. Fix: publish chaos until the first
counterexample; a counterexample the raw table already classifies
correctly switches publication without spending budget (index still
strictly grows, so round progress holds). The failing seeds are
pinned as a regression test. *Spec impact:* §3.2's "publication"
paragraph should state the chaos-until-first-counterexample rule
explicitly; it currently permits the unsound reading.

**F2 — the paper's §6 narrative is search-order dependent.** With BFS
in event order, the first witness for `clients(2)` is `g1·g1`, not
the paper's `g1·g2` — and the *client* also rejects `grant·grant`,
so both the server and the (shared) client entry are culprits, and
both end exact. "The clients are never touched again" holds only for
the witness the paper happened to choose. What survives, and what the
data shows: interning makes the refinement count K-independent (all
clients are one entry), and the verdict/certificate are unaffected
(T1–T3 are policy- and order-independent, as claimed). *Paper
impact:* §6 should either pick the witness deliberately ("a search
returning `g1·g2`…") or report both culprits; the phenomenon to
advertise is entry-count, not leaf-count, locality.

## T-A — verdict parity (O1 as data)

`ta_tb_parity.tsv`: families (clients, clients-bug, ring × sizes
{2,3,5,8}) + two rand grids × 100 seeds each. **212 rows: 56 holds,
156 violation; 212/212 agree with `mono`; 56/56 certificates pass
`hsc-certcheck`; 0 timeouts.** Every violation witness refires
concretely (the driver hard-fails otherwise; 0 such failures).

## T-B — budget and the emergent cone (O3 as data)

Over all 212 rows, **counterexamples ≤ budget everywhere** (0
violations of the T3 bound). Cone on the 48 rand `holds` rows
(distinct-leaf entries): **71 chaotic / 6 intermediate / 81 exact** —
about 45 % of entries are never probed by the property at all, and
the loop leaves them at the free rung. Abstract states walked on
those rows total 80 vs 95 for `mono` — at these model sizes the two
walks are comparable; the separation argument is T-C.

## T-C — symmetry scaling

`tc_scaling.tsv`, clients/ring × n ∈ {2..64} × intern {on, off}.

* **ring is the advertised phenomenon**: with interning, rounds = 3
  and counterexamples = 4 at *every* N from 2 to 64 (one shared
  station entry + station 0); with interning off, rounds = N+1 and
  counterexamples = 2N. Refinement effort is size-independent
  exactly when the symmetry is shared; the verdict never changes
  (ablation confirms policy-independence of T1–T3).
* **clients prices the promise honestly** (paper §7): the server is
  the property's enforcer and its exact contract has 2K+2 classes,
  so refinement rebuilds it wholesale — cex = K+1, and walltime grows
  to 4.2 s at K=64 while `mono` stays at milliseconds (the concrete
  product is *small*: 2K+1 states — clients are tightly coupled to
  the server, so there is nothing for abstraction to skip). The loop
  wins when the property is local, ties-with-overhead when it is
  not; this family is the "not".
* |Inv| = mono states on both families at every size: the final
  abstraction is exactly as coarse as the property allows.

## T-D — policy knobs

`td_policies.tsv`, 50 seeds × {all, first, cheapest} × jump-exact
{off, on}: **no separation** — only 7/300 runs refine at all (the
rand corpus is violation-dominated; witnesses are real on round 1),
so all configurations coincide within noise (~2.4 ms/run). The knobs
need a refinement-heavy corpus to be measurable; question returned
to Theory (spec §10 revision).

## Deviations from spec

* `run --tsv` column order is verdict-first (spec §10 lists model
  first; the sweep script prepends model columns, net format as
  specified).
* The in-process test for O5 checks the emitter's shape only; the
  mutation battery runs out-of-process in the sweep (spec allows
  either placement).
