# Handoff — explicit engine & the rewrite chain

Current state, pointers only. Delivered and documented: the explicit
engine (`include/hsc/xpl/`, docs-first: README + algorithm.md +
interpret/algorithm.md), the runner layering (translator symbolic-only,
`src/surface_run.cc` knows both engines), the spec-as-data module
(`surface/spec.hh`: declarations + domain inference), and the rewrite
chain (`surface/algorithm.md` §1c): `simplify-constants` (default),
`hotbit` (automaton discipline: pinned writes only), `reorder-force`
(FORCE per level, hierarchy kept, tops-biased cost `hi − 2·lo` in
`order/force.cc` — the composite heuristic's `2·max − min` mirrored),
`flatten`, `simplify-arrays`, `decompose-louvain` (Louvain shared as
`hsc_louvain`), and the `(print-spec)` command. `hsc` CLI: `--explicit`,
`--domains`, `--rewrite`, `--cap`.

Evidence so far (all at 15 s/model, baseline
`examples/divine/runs/20260724T203500Z_7138020_sym15.tsv`):

* engines: explicit 204 run-ok vs symbolic 177; 167/167 counts agree;
  union 214/276 (archives under `examples/divine/runs/`).
* hotbit strict: 147 models, 736 vars, 147/147 counts agree, final
  nodes worse on all 147 **standalone** (`tests/logs/hotbit_strict_sweep.tsv`).
* louvain(+force, tops cost) spot checks: lamport.1 568→251,
  bopdp.1 1093→524 nodes; at.1 worse. Counts invariant everywhere.

## TODO — engineering (next action first)

1. **Score the two in-flight sweeps** (results in
   `tests/logs/louvain_sweep.tsv` — louvain+force over all 276 — and
   `tests/logs/hotbit_force_sweep.tsv` — hotbit+force over the 253
   candidates in `tests/logs/hb_candidates.txt`). Compare counts
   (must agree), nodes fewer/same/more, newly-solved vs lost against
   the sym15 baseline. Re-run with `tests/rw_sweep.sh LABEL 'DIRS'
   [list]` if partial.
2. `xreach` should accept an EVTERM argument (v1 gap; the runner owns
   the command).
3. Deadlock query in the explicit engine (`quick` is exact on
   guard-form events; visitor stops at first hit).
4. Projection-differential harness: strip hidden coordinates in the
   explicit engine, compare projected reachable sets — the oracle for
   every structural reduction (counts are not preserved there).
5. Deferred engine refinements, measure before adopting: effect
   classes, directional wake-up (`xpl/algorithm.md` §3), parallel
   explicit (interfaces are cut for it), guided walks.

## TODO — theory

* `research_notes/structural_reductions.md`: agglomeration criteria
  (pre/post, the persistence-vs-trivial-consumer disjunction) and
  stubborn-set POR over the supports+domains analyses. Refine into a
  spec; decide v1 kernel (proposed: pre-agglomeration under the
  trivial-consumer condition, fixpoint with re-analysis).
* Hotbit pin-sets of size ≤ 2 (one clear per possible prior bit, still
  reset-free) if the strict rule proves too tight on data.
* Louvain graph strategy knobs (guard-read-and-written positions as
  targets; weaker read edges) — measured, not assumed.

Blockers: none.
