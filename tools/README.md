# `tools/` — the command-line binaries

| binary | role |
|---|---|
| `hsc` | run a `.hsc` session: files and `-e` forms spliced in order (`doc/hsc_manual.md`) |
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
       [--shape nupn|flat|louvain] [--force] [--reverse] [--invariants S] [--bound N]
       [--states | --max-tokens] [--deadlock NAME] [--totalTime S] [--printUnknown] [--witness]
       [--export-hsc FILE] [-q] [-v]
```

### Pipeline

1. **Load.** `-i`: the vendored PNML loader plus the NUPN unit tree when the
   file has one. `--net`: `PNETIO::read`, places `p<i>`, transitions `t<i>`.
2. **Shape.** `nupn` (default with `-i`, falls back to `flat` without a unit
   tree), `flat` (one spine), `louvain` (the clustering of `decompose.hh`);
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
| `(bound N E [K])` | `(select Q_k R (>= E' k))` for k by binary search over `[0, bound)` | `FORMULA N <max>` |
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

Bounds: the surface has no per-expression maximum, and the leaf domain
`[0, bound)` is known, so the maximum of `E` over `R` is found by binary
search on `k` with one `select` per probe: at most `log2(bound · Σ|c|)`
probes, each a symbolic operation on the fixpoint. A hint `K` is checked
first: `E >= K` non-empty answers `K` in one probe.

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

### `--states`

The four values of the MCC StateSpace examination, each its own line:
`STATES` from `(count R exact)`, `MAX_TOKEN_IN_PLACE` from `(max-value R)`,
`MAX_TOKEN_PER_MARKING` by the binary search above on the sum of all places,
`TRANSITIONS` as Σ_t `m(t)` · `(count (select R G_t) exact)`: the enabled
pairs, weighted by the `TMULT` block of the PNET when it carries one (how
many transitions of the producer's original net each transition stands for,
1 without a block — INTEROP.md section 3). Transitions that cannot change
the marking are left out of the emitted model, since they add nothing to the
fixpoint; their guards are read from the net, here and for the deadlock atom.
`--max-tokens` prints the `MAX_TOKEN_IN_PLACE` line alone: the OneSafe
examination is TRUE iff it is at most 1.

### Budget and failure

The engine is not interruptible; `--totalTime S` arms `alarm(S)`, and the
handler writes `UNKNOWN <name>` for every property still open when
`--printUnknown` is set, then exits 0. A leaf overflow (`overflow_error`, a
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
