# The order sweep — what we will look at, hence what we collect

Design note, written before the first cluster job. The pages come first
(what a comparison of orders should let us see), the record per run follows
from them, the job script from the record.

## 1. The question

Many heuristics, one fixed budget each (300 s), one run per (instance,
examination, heuristic), no competition inside a run. We are not looking for
the best average: we are looking for heuristics that **overcome some model
the others cannot**, then for the structure of those models, then for
heuristics **dominated** everywhere (to discard). Averages hide both.

The primary target is the **reachable set**: once `R` is built the rest
follows, and without it the symbolic approach has nothing. So the sweep runs
the StateSpace examination on every P/T instance — the four values, the
transition count included, graded against the oracle where it has one — and
the two CTL examinations beside it; every CTL run builds `R` first and
records its cost, so the reachability study comes with the CTL one, whose
formulas carry the contest's own bias. The population is **all 1681 P/T
instances**, the 809 above 10^9 states and the 449 of unknown size included:
exponents past ten are where a symbolic engine earns its place against the
explicit tools, one firing discovering exponentially many states.

A second population, later: the **reduced nets** ITS-Tools hands its engines
in its own scenarios (structural reductions first) — closer to what libHSC
sees in production than the raw contest models; the deploy tree's `reducer`
is the way to collect them.

## 2. The pages (one per examination, as `MCC-analysis/campaign` does)

1. **Heuristics against each other** — one row per heuristic:
   answered, complete instances, wrong (must be 0), timeouts, memory
   failures (the 15 GB rule), median / p90 wall, median / p90 peak RSS,
   **unique wins** (instances where it alone has the most answers),
   **marginal contribution** (answers the virtual best loses without it),
   **dominated by** (the heuristics that answer at least as much on every
   instance — a non-empty cell is a discard candidate). Default sort:
   marginal contribution, then unique wins. This is the table that picks
   the portfolio.
2. **Pairwise matrix** — heuristics × heuristics, cell = instances where the
   row answers strictly more than the column; a click selects the pair for
   the views below. Asymmetric cells with a large value on one side only
   are dominance; large values on both sides are complementarity, the
   interesting case.
3. **Instances** — one row per instance, one column per heuristic, the
   answered count as a heat colour (0 = dark, 16 = light), a metric
   selector switching the cell to: time to complete (∞ when not),
   `R` time, `R` nodes, peak RSS, belly (the widest level's nodes). Sort by
   **spread** (max − min answered across heuristics: where the order
   matters), by **hardness** (best answered, ascending: what nobody
   solves), by family. Filters: family regex, "some heuristic complete,
   another empty", "wrong somewhere". Each cell links to the run's log.
4. **Scatter A against B** — one point per instance, log axes, the selected
   metric (time to complete, `R` nodes, RSS); colour by who answered more;
   the outliers off the diagonal are the instances to read by hand.
5. **Cactus** — per heuristic, instances completed within t, t on the x
   axis; and the same with answers instead of instances (from the answer
   timestamps), which shows a heuristic that answers early but never
   finishes against one that is slow to start and completes.
6. **Families** — family × heuristic, the mean rank of the heuristic inside
   the family (1 = best), as a heatmap. A heuristic that wins one family
   and loses the rest is a portfolio member, not a default.
7. **One instance in depth** — for a chosen instance: the profile of `R`
   under every heuristic (nodes per level, overlaid, x = level rank), the
   dependency matrix as a spy plot under each order (places × transitions
   permuted by the order: bandwidth made visible), the flows and the
   protected events. This is where "why did Sloan win here" gets answered.

## 3. The record per run

One TSV line per (instance, examination, heuristic), logs kept beside it
(`<tag>/<instance>-<exam>-<heuristic>.{out,err}`). Columns:

| column | source | for |
|---|---|---|
| `instance`, `family`, `exam`, `heuristic`, `tag` | the job | keys |
| `shape_sig`, and the shape file `<run>.shape` beside the log | `hsc-pn --shape-only --export-shape` | duplicates folded, page 7 |
| `states` | the oracle's StateSpace count | hardness axis |
| `wall_s`, `cpu_s`, `maxrss_kb`, `rc`, `status` | `/usr/bin/time`, the exit: `ok`, `timeout`, `memory` (killed by the 15 GB `ulimit -v`), `crash` | pages 1, 4 |
| `answered`, `ok`, `wrong`, `unknown`, `complete` | the FORMULA lines against the oracle | pages 1–3 |
| `answer_times` | `hsc-pn -v`: `answered <name> at <s>` per verdict, joined `name:s;…` | page 5, per-formula differentials |
| `reach_s`, `reach_nodes`, `reach_arcs`, `reach_states`, `partial` | the `stats` line of `hsc-pn -v`; the states and nodes reached even when the budget cut the set (partial = 1) | pages 3, 4, 7: a `noreach` run still says how far it got |
| `belly_nodes`, `belly_level`, `belly_span` | the widest level of `(profile R)` | page 3, 7 |
| `shape_depth`, `shape_units`, `shape_widest_unit` | the unit tree | page 7, explaining hierarchy effects |
| `flows`, `flows_widest`, `flows_maxconst`, `protected` | `-v` lines | page 7 |

The full profile (all levels) stays in the log, read by page 7 on demand.

## 4. The heuristics of the first sweep

| name | hsc-pn flags | environment |
|---|---|---|
| `nupn` | `--shape nupn` | |
| `flat` | `--shape flat` | |
| `random` | `--shape random --seed 1` | the control |
| `rcm` | `--shape rcm` | |
| `sloan` | `--shape sloan` | |
| `force` | `--shape nupn --force` | |
| `force-rev` | `--shape nupn --force --reverse` | |
| `rcm-force` | `--shape rcm --force` | FORCE started from a bandwidth order |
| `sloan-force` | `--shape sloan --force` | |
| `louvain` | `--shape louvain` | |
| `louvain-force` | `--shape louvain --force` | the current best |
| `louvain-force-rev` | `--shape louvain --force --reverse` | |
| `louvain-inv` | `--shape louvain --force --invariants 5` | flows as cliques |
| `louvain-inv5` | the same | `HSC_INV_WEIGHT=5` |
| `louvain-merge4` | the same | `HSC_INV_MERGE=4` |
| `louvain-merge2` | the same | `HSC_INV_MERGE=2` |
| `force-iters` | `--shape louvain --force` | `HSC_FORCE_ITERS=2000` |

Seventeen, one budget each; all 1681 P/T instances (`models_pt_all.txt`)
on StateSpace, CTLCardinality and CTLFireability: 1681 × 3 × 17 = 85 731
runs of at most 300 s. One job per (instance, examination) running the
seventeen in sequence: 5043 jobs of at most 85 minutes each, one core each
(the tool is single-threaded), `ulimit -v 15000000` per run, `--totalTime` 30 s under the `timeout`,
`--cover` on the StateSpace runs (76 instances of the corpus are unbounded;
the divergence watch answers them `+inf` when a pumping pair is found)
(the margin belongs to the job: 21 runs of the first sweep outlived a 2 s
one; the systemic answer is `include/hsc/sched/algorithm.md`). StateSpace
is submitted first. A run's `status` is `ok`, `timeout`, `memory`, `crash`,
or `noreach` — the budget spent inside the reachable set, nothing answered.

## 5. The RAM rule

The nodes kill a one-core job at about 6 GB resident (SIGKILL, nothing
said; observed at 6005 MB on every killed run of the first sweep). We do
not fight it: what can be done within 6 GB is the question worth answering,
and a kill is recorded as `status=memory` (rc 137). The job's own
`ulimit -v 15 GB` stays as a backstop. Peak RSS is collected for every run
(`/usr/bin/time %M`); its p90 per heuristic is a column of page 1 and a
discard criterion of its own.

## 6. Identical shapes

Heuristics coincide often (`nupn` is `flat` on a net without units, the
flow variants agree when no flow is heavy). `hsc-pn --shape-only` prints the
signature of the rewritten shape without building anything; the job runs the
first heuristic of a signature and records the others as `dup:<name>`, and
the pages should read them as one column. The shape itself is a file:
`hsc-pn --export-shape FILE` writes the rewritten `(shape (spine …))`
expression over the place names, `--shape-file FILE` takes one — the
reified order and hierarchy, human-readable, diffable, tweakable by hand,
buildable by any tool that writes parentheses. The sweep should export it
beside every run, so page 7 can show and diff the shapes that won.

## 7. The epoch experiment that follows

With the stop state and the surface orders in place (`sched/algorithm.md`):
run every heuristic for a fixed budget through `hsc FILE --stdin` with
`(budget S) (reach R saturate) (stock R)`, compare the stocks across shapes
by what is comparable — the leaf domains, the states, the local states of
the subshapes that both shapes have — then fuse the sets different shapes
reached (rewrite to one shape with `(use-shape …)`, join) and continue from
the best. The first scenario of the coordinator, by hand.

## 8. What decides the next sweep

Discard the dominated. Read the unique wins by hand (page 7). Turn a
recurring pattern into a rule (a contraction threshold, a FORCE seed, a
belly-driven reshape), and sweep again on the instances where the spread
was large — the others do not discriminate.
