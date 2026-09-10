# `tools/` — the command-line binaries

| binary | role |
|---|---|
| `hsc` | run a `.hsc` session: files and `-e` forms spliced in order (`doc/hsc_manual.md`) |
| `hsc --stdin` | the same engine driven by another tool: files and `-e` forms first, then orders read from standard input one form at a time, each answered and flushed — `(budget S)`, `(reach R saturate)`, `(stock R)`, `(reach R2 saturate from R)` make the epoch loop a conversation (`include/hsc/sched/algorithm.md`) |
| `hsc-pn` | answer Petri net properties symbolically: PNML or PNET in, MCC XML or s-expression properties in, the `FORMULA` line protocol out |
| `hsc-mcc` | the MCC examinations StateSpace and OneSafe on a PNML net (superseded by `hsc-pn`, kept while the harness driver moves over) |
| `nupn2hsc` | import a PNML/NUPN net and emit the `.hsc` model (text only) |
| `dve2hsc`, `fsp2hsc` | the DVE (BEEM) and FSP (LTSA) importers |

The PNML tools need expat and are skipped without it. `check_samples.cmake`
is the `mcc_samples` test: every tool answer on `examples/mcc` against the
MCC oracle.

## `hsc-pn`: design

One binary for two input pairs, the contest's and the tool-to-tool one of
PetriSpot's `INTEROP.md`, meeting in the same property tree
(`hsc/petri/expr/Property.h`) before anything is translated. The MCC
examination protocol (examination names, the model folder) is a wrapper's
business (`MCC-drivers/hsc/`); this tool answers properties.

```
hsc-pn (-i model.pnml | --net model.pnet) [--props FILE] [--propsSyntax auto|mcc|sexpr]
       [--shape nupn|flat|louvain|rcm|sloan|random | --shape-file FILE] [--seed N] [--force] [--reverse] [--invariants S]
       [--export-shape FILE] [--shape-only] [--bound N]
       [--approx S [--approx-only] [--approx-units] [--approx-back K] [--approx-back-time T]] [--dead S [--dead-step]]
       [--states | --max-tokens] [--deadlock NAME] [--totalTime S] [--printUnknown] [--witness]
       [--export-hsc FILE] [-q] [-v]
```

### Pipeline

1. **Load.** `-i`: the vendored PNML loader plus the NUPN unit tree when the
   file has one. `--net`: `PNETIO::read`, places `p<i>`, transitions `t<i>`.
2. **Shape.** `nupn` (default with `-i`, falls back to `flat` without a unit
   tree), `flat` (one spine), `louvain` (the clustering of `decompose.hh`),
   `rcm` / `sloan` (flat, in a bandwidth-reducing order of the place
   dependency graph, `order/bandwidth.hh`), `random` (flat, a seeded
   shuffle — the control);
   The reachable set runs under four fifths of `--totalTime` as a
   cooperative deadline (the alarm stays the backstop): when it runs out the
   set is partial and the run says so. Every state in a partial set is
   reachable, nothing says the rest is not, so it answers only what a state
   found in it decides: a reachable goal, a violated invariant, a deadlock
   found, a CTL formula with E operators only that holds or with A operators
   only that fails (`ctl/algorithm.md` §7); bounds, StateSpace values and
   everything else stay `UNKNOWN`.
   Under `-v`, once the reachable set is built or cut, one `hsc-pn: stats`
   line gives its cost and size (`reach_s`, `partial`, `reach_states`,
   `reach_nodes`, `reach_arcs`), its
   widest level (`belly_nodes`, `belly_level`, `belly_span`) and the shape
   (`shape_depth`, `shape_units`, `shape_widest`); every verdict then adds
   `hsc-pn: answered <name> at <s>` — the record the order sweep collects
   (`experiments/order/SWEEP.md`).
   **The shape as a file.** `--export-shape FILE` writes the shape after
   the rewrites as one `(shape (spine …))` expression over the place names;
   `--shape-file FILE` takes such a file (with or without the `(shape`
   wrapper) as the shape, overriding `--shape` — the reified order and
   hierarchy every heuristic ends in, human-readable, diffable, buildable by
   any other tool. `--shape-only` builds and rewrites the shape and prints
   its signature (`hsc-pn: shape sig=<hex>`, also in the `-v` stats line as
   `shape_sig`), no fixpoint: two heuristics with one signature produced
   the same shape.
   `HSC_REACH_TRACE=1` times the closure's construction and its application
   on stderr (an observation point).
   `--force` appends the `(reorder-force)` directive, `--reverse` the
   `(reorder-reverse)` one after it (the mirror image of the shape: which
   end of an order sits at the top matters to the engine). `--invariants S`
   computes the net's P-flows within S seconds (`petri/invariants.hh`) and
   hands them to the `louvain` shape as extra hyperedges; `-v` reports how
   many, the widest support and the largest constant. The model is emitted as
   `.hsc` text by `to_surface` and parsed back: the surface is the one
   language, the tool never talks to the calculus directly.
3. **Properties.** `loadPropertyFile` (syntax by extension or forced), one
   `Property` per form: kind, name, body, CTL formula, bound hint.
4. **Queries.** `props_to_surface.hh` turns every property into surface forms
   over the reachable set `R` (table below), each answered by one `count`.
   The forms follow `(reach R saturate)` in one session, so every property
   reads the same fixpoint. `translate()` writes its lines to a decoding
   stream: each `count` line that arrives is turned into a protocol line at
   once, so verdicts stream while later queries still run.
   With a CTL property in the file the model keeps the transitions that move
   no token (read arcs, self-loops): they are edges of the reachability graph
   — a deadlock they enable is none, a cycle they close is one — and only
   the fixpoint can do without them.
   CTL properties run after the others under **fair shares**: a property
   may take twice the remaining budget divided by the properties still
   open, what it leaves is shared again by a second pass, and a last pass
   gives the rest to one — so an expensive formula cannot take the whole
   budget from the cheap ones, and the work lost at an interruption (all
   but what was memoised) happens at most twice per property. Without
   `--totalTime` they run in order to their end.
5. **Answers.** The line protocol of `INTEROP.md` section 5 on stdout,
   flushed per line; the loader's log and every diagnostic on stderr.

### Property to query

| property | surface forms | answer |
|---|---|---|
| `(reach N B)` / EF | `(select Q R B')` `(count Q)` | TRUE if the count is non-zero, else FALSE |
| `(invariant N B)` / AG | `(select Q R (not B'))` `(count Q)` | FALSE if non-zero, else TRUE |
| `(deadlock N)` | `(select Q R (not (or G_1 … G_n)))`, `G_t` the guard of `t` | TRUE if non-zero |
| `(bound N E [K])` | `(max-sum R (* c p)…)`: the maximum of the form over `R` in one pass over the diagram | `FORMULA N <max>` |
| `(ctl N F)` | `(ctl Q F')`, `F'` the formula in the surface's CTL grammar (manual §8f) | the session's `Q ctl TRUE|FALSE`; `UNKNOWN` leaves the property open |

`B'` is the body printed in the surface's atom syntax: `(and|or|not …)`,
comparisons `== != <= >= < >`, a linear form as a right-nested binary sum
`(+ (* c p) (+ …))` with `(* c p)` only for `c ≠ 1` and a bare place for a
single unit term. Constants: PetriSpot's `simplify` folds a body to
`True`/`False` before we see it, and such a property is answered without a
query (`TECHNIQUES TOPOLOGICAL TRIVIAL`), the surface having no constant
atom. A deadlock on a net with a transition whose preset is empty is FALSE
the same way. `fireable` is already desugared into pre-arc comparisons by
the parsers. Place references are the leaf names of the emitted model, which
are the net's place names on both paths.

Bounds: `(max-sum R terms)` is one memoised bottom-up pass over the
diagram (the best arc is the head's best weighted value plus the tail's
best), linear in the nodes, exact — where a binary search on selections of
a sum over every place used to stall (1091 runs of the first sweep built
`R` and never got `MAX_TOKEN_PER_MARKING`). The UpperBounds hint is only
compared to the result under `-v`.

### `--witness`

Every verdict that rests on a non-empty set gets its witness on
**stderr** — stdout keeps the `FORMULA` protocol alone. The two kinds are
one idea: a run of the net, found symbolically (`hsc/trace/`, manual §8g),
states as words with their zero places dropped (`((p1 2) (p3 1))`), events
as transition names.

* **Reachability, invariant, deadlock**: when the selection is non-empty
  (`TRUE` for a reachability or deadlock property, `FALSE` for an
  invariant — its counterexample), `WITNESS <name> path K` then a shortest
  run from the initial marking (fed once as `(word hsc-pn-I …)`) to a
  selected state, K steps, states and transitions alternating.
* **CTL**: after the verdict, the surface's `(witness Q)` tree:
  `WITNESS <name>`, then, indented by subformula depth, the runs the
  forward form stands for and `;` notes saying what holds where or why no
  path is shown (a universal subformula holds by exhaustion; a `TRUE`
  verdict of a universal property has no path). A witness that runs into
  the deadline prints `WITNESS <name> TIMEOUT`.

The inverted events the backtrack needs are built on the first request,
once per run; bounds have no witness.

### `--cover`

The divergence watch. A place whose value runs past the largest initial
marking is a place beyond anything the model started with: the leaf theory
notes it, asks the closure to return (a partial set, `R diverged`), and
doubles the limit so the next report comes at the next doubling. With
`--cover`, each such epoch asks `(pump R)` — a pumping pair (manual §8g)
proves a place unbounded — and, found, answers the four StateSpace values
`+inf` with the technique `COVERABILITY`; not found, the closure resumes
(`(reach R saturate from R)`) under the same deadline. A set cut by the
deadline without a note gets one last pump. CryptoMiner-PT-D05N000 answers
in 0.03 s and FunctionPointer-PT-a008 in 0.12 s at the first doubling;
BugTracking-PT-q3m016 (754 places, 27 370 transitions, most dead) meets its
runaway place under the Sloan order only and its pump runs out of time —
a net for ITS-Tools to reduce first.

### `--approx`, `--dead`

The over-approximation first (`include/hsc/linear/`, ideas #19). `--approx S`
runs a pass of its own (`tools/pn_approx_pass.hh`) before the fixpoint: the
net is **abstracted** to the places the linear facts bound — the others
removed with their arcs (`tools/pn_abstract.hh`), which only adds
behaviour, so every impossibility proved there holds of the original net
for a question over kept places — its model emitted under the Sloan order
with leaf domains as wide as the box, in a session separate from the main
one; there the invariant set `S ⊇ R` is built: the
P-flows within S seconds, the positive ones as bounds and equality
diagrams, the never-marked places bounded by 0, every place bounded by 1
when the PNML carries a safe NUPN tag, and with `--approx-units` one token
at most per NUPN unit (`(at-most …)`); the box `F`, the constraints, `S`
their meet (`tools/pn_approx.hh`). Then every open reachability, invariant
and deadlock property is tried on `S`: a goal that selects nothing of `S`
is unreachable — `FALSE` for a reachability, `TRUE` for an invariant,
`FALSE` for a deadlock (only when no place was removed) — with the
technique `TOPOLOGICAL`; a goal that reads a removed place, or that `S`
does not rule out, stays open and goes to `R`. `--approx-only` stops there (UNKNOWN for the rest). The set
is built without a deadline: a partial set would not over-approximate.
`--approx-back K` then runs, for every property `S` alone leaves open, a
backward search inside `S` from the goal's markings (`(backward …)`,
manual §8h), up to K layers under `--approx-back-time` seconds each: a
layer that meets the initial marking is a real path (the converses are
exact) — of the net the pass runs on, so the goal is reachable only when
that net is the original one: an abstraction has more behaviour, and its
paths need not exist, so with removed places the reachable verdict is not
taken; a search that closes with nothing left proves the goal unreachable
— sound because every place of the abstract net is exact in `S`.
Technique `K_INDUCTION`.
`--dead S [--dead-step]` runs the same pass for the transitions dead on
`S` (`DEAD_TRANSITIONS …`, names under `-v`; with `--dead-step` the
one-step test backward per slice), no fixpoint.
`hsc-pn: approx …` on stderr is the record: flows, covered places, zeros,
unit constraints, the sizes of `F` and `S`, the times, the count refuted.

### `--states`

The four values of the MCC StateSpace examination, each its own line:
`STATES` from `(count R exact)`, `MAX_TOKEN_IN_PLACE` from `(max-value R)`,
`MAX_TOKEN_PER_MARKING` from `(max-sum R)` (every place, coefficient 1),
`TRANSITIONS` as Σ_t `m(t)` · `(count (select R G_t) exact)`: the enabled
pairs, weighted by the `TMULT` block of the PNET when it carries one (how
many transitions of the producer's original net each transition stands for,
1 without a block — INTEROP.md section 3). Transitions that cannot change
the marking are left out of the emitted model, since they add nothing to the
fixpoint; their guards are read from the net, here and for the deadlock atom.
`--max-tokens` prints the `MAX_TOKEN_IN_PLACE` line alone: the OneSafe
examination is TRUE iff it is at most 1.

### Budget and failure

`--totalTime S` is a wall budget from the first line of `main`: a SIGALRM
armed before the parse, whose handler writes `UNKNOWN <name>` for every
property still open when `--printUnknown` is set, then exits 0. Inside the
session the CTL properties run under cooperative deadlines (the manager's
interrupt hook, polled at the closure loops); the other phases do not poll
yet. This is a backstop, not a design: the budget object that replaces it —
one deadline for time and memory, polled everywhere, tasks resumable, in
PetriSpot's scheduler vocabulary — is `include/hsc/sched/algorithm.md`. A leaf overflow (`overflow_error`, a
place beyond `--bound`) aborts the session: the properties without a verdict
are `UNKNOWN`, never a wrong answer; the caller may retry with a larger
bound. Exit code 0 when the run ends, non-zero for a bad file, an unresolved
reference or an unknown option.

### Checks

`examples/mcc/`: the same properties on both paths. `<Model>.<Exam>.xml`
are the contest files, `<Model>.pnet` and `<Model>.<Exam>.sexpr` their
tool-to-tool forms written by `petri64 --exportNet` and
`--printProps=sexpr-index`, `oracle/<Model>-<EXAM>.out` the contest oracle.
`check_samples.cmake` runs `hsc-pn` on PNML plus XML and on PNET plus
s-expressions and compares every `FORMULA` line with the oracle.
