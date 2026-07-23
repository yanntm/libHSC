# Parametric surface — spec

Binders and compile-time parameters for the `.hsc` surface: express a family
of N identical components once, as one template. Design settled in discussion;
this file is the engineering contract. Companion report:
`hsc_parametric_report.md`.

## 0. Principle

A binder carries **no semantics of its own**: it generates a list in place,
and the *position* of that list decides its meaning — exactly as for explicit
lists today. `forall` expands through the context's conjunctive combinator
(seq / and / clause-splice), `exists` through the disjunctive one (alt / or /
event family). The expansion is literal; the denotation is the existing event
algebra's. Nothing new to prove at the calculus level.

Implementation is a **datum → datum pass** between parser (T2M) and
translator (M2M) — the pass the surface README already anticipates. The
translator is unchanged. A `--expand` flag dumps the expanded tree as plain
`.hsc` text: the degeneralization is a feature, diffable against generator
output (`tools/hanoi.py`), and the expanded file must itself run identically.

## 1. Grammar additions (all removed by expansion)

```
(param NAME EXPR)             ; compile-time integer; NAME usable where INT
                              ; stands below this form; shadowing/collision
                              ; with a declared name is an error
(array NAME COUNT [LO HI])    ; COUNT folds to an INT: declares cells
                              ; NAME_0 … NAME_{COUNT-1} (with bounds if
                              ; given) and groups them, i.e. lowers to
                              ; (leaf NAME_i [LO HI])* (array NAME NAME_0 …)
                              ; The explicit-cell form (array NAME CELL+) is
                              ; untouched; the forms are distinguished by
                              ; whether the third item folds to an integer.

BINDER ::= (NAME EXPR) | (NAME EXPR EXPR)   ; range [0,HI) resp. [LO,HI);
                                            ; bounds fold to INTs and may
                                            ; reference enclosing binders
(forall BINDER DATUM+)
(exists BINDER DATUM+)
```

Substitution: an atom equal to an in-scope `param` or binder name is replaced
by its integer value, everywhere except name-binding slots. The expander
**constant-folds** integer arithmetic (`+ - * / % << >> & | ^ ~`) bottom-up,
so `(% (+ i 1) N)` grounds once `i` is substituted. Division by zero at fold
time is an error with the form's line.

Grounded access to a *generated* array — `(at NAME K)`, K an integer literal
after folding, 0 ≤ K < COUNT — is rewritten to the cell atom `NAME_K`. This
makes generated cells usable in `shape` and `init`/`word` pair positions,
where a bare leaf name is expected, with no translator change. Out-of-range
grounded K on a generated array is an expansion error (the bound is known);
accesses to hand-declared arrays and open indices pass through untouched
(existing lia semantics, including ⊥ on dynamic out-of-bounds).

## 2. Expansion by context

For each syntactic context, `forall` expands via the conjunctive route,
`exists` via the disjunctive one — realized as a **splice** when the
context's own combinator already matches, as a **wrapped node** when it does
not, and as an **event family** at event-clause level:

| context (list position)        | forall                    | exists                  |
|--------------------------------|---------------------------|-------------------------|
| event body clauses             | splice (body is a seq)    | family: NAME_v events   |
| `seq` body                     | splice                    | wrap `(alt …)`          |
| `alt` body                     | wrap `(seq …)`            | splice                  |
| single EVTERM slot (`reach`, `apply`, `init`) | wrap `(seq …)` | wrap `(alt …)`     |
| `when` body, `and` body, select QATOMs | splice            | wrap `(or …)`           |
| `or` body                      | wrap `(and …)`            | splice                  |
| single BEXP slot (`not`, `imply` child) | wrap `(and …)`   | wrap `(or …)`           |
| `do` body (one clause)         | splice — **simultaneous** | error, points to clause level |
| `spine` / `balanced` sorts     | splice                    | error                   |
| `init` / `word` pair lists     | splice                    | error                   |
| top-level forms                | splice                    | error                   |
| EXPR position                  | error                     | error                   |

A binder body of several datums expands, per index value, to that list; the
per-instance lists concatenate under splice, and wrap as `(seq …)` (resp. the
whole instance list under one `(alt …)` summand each) when wrapping.

**The two loop flavors, by position** (the load-bearing distinction):

```
(forall (i N) (do (:= (at a i) (at a (% (+ i 1) N)))))  ; N sequential steps
(do (forall (i N) (:= (at a i) (at a (% (+ i 1) N)))))  ; one clause: a true
                                                        ; simultaneous rotation
```

Sequential composition is always defined (last write wins); the simultaneous
clause takes the existing clause rules (fuse if separable, one case bracket
otherwise). A `seq` of instances with pairwise-disjoint supports denotes the
same diagram as the fused product; recognizing and fusing that is a possible
later optimization, to be measured first, never a semantics question.

**Event families.** An `exists` at event-clause level lifts to the event
level: `(event take (when G) (exists (i N) (do A_i)))` becomes the plain
events `take_0 … take_{N-1}`, each `(when G) (do A_i)`, every one entering
the default system sum — so `reach`'s default (ALT of every event) includes
the family, with no translator change. Precedent: libITS ANY delegators
expand to `n` syncs named `sname + i`. Several `exists` at clause level
compose as a cross product, suffixes in order of appearance (`move_2_0_1`).
A `forall` at clause level whose body contains an `exists` crosses per index
(for each i, some j) — legal, potentially large, the user's own request.
Instances whose guard folds to false are zero terms and vanish from the sum
(existing rule) — degenerate instances (`i == j`, `a == b`) cost nothing.

**Empty ranges**: splice contexts splice nothing; wrap contexts emit the
identity filter `(when)` for `forall` and `(abort)` — the zero term, added
to the base grammar alongside this work — for `exists`. An event whose body
expands to nothing is an error; an event family over an empty range emits
no events (the zero term in the sum, honestly).

## 3. What is deliberately deferred

- **structs** (named component types with fields): parallel arrays suffice
  for the target models; structs add ergonomics + a layout guarantee, later.
- **`seqfor`** (sequential loop with carried dependence, reductions): well
  defined (`seq` in index order), no current use case.
- **scalarset discipline checker + orbit tags**: the binder-only, uniform-
  pattern subset is a symmetry certificate (Murphi / libITS lineage) enabling
  quotient reduction — paper-sized, later. The expander must simply not
  *obstruct* it (keep the template → instance mapping reconstructible).
- **CLI parameter override** (`-DN=12`): trivial, add on demand.
- **Fusing provably-disjoint seq instances into one product**: measure first.
- **Multi-dimensional arrays** — proposed next increment, pure expander
  sugar: `(array m N M [LO HI])` declares cells `m_i_j`; `(at m I J)` with
  all indexes grounded folds to the cell, with any index open lowers to the
  flat 1-D access `(at m (+ (* I M) J))` — row-major linearization, exactly
  C's. No calculus or translator change; the *layout* of the cells stays
  the shape's business (rows as blocks, etc.), keeping declaration and
  representation orthogonal.
- **`ite` as a statement / general EVTERM event bodies**: GAL's
  `for/if/abort` uses are subsumed by quantified guards; a genuine
  branching update is expressible today as
  `(alt (seq (when c) S₁) (seq (when (not c)) S₂))` at term level. If a
  model wants that *inside* an auto-summed event, the clean home is
  generalizing event bodies to EVTERM+ — a compiler change, staged.
- **`select` with quantified disjunctions**: a select atom is pinned to one
  leaf or relates two; an `(or …)` across several leaves is refused. The
  workaround is canonical (a `(when …)` filter + `apply`); routing rich
  BEXPs through the case engine in `select` itself is a possible unifier.

## 4. Milestones

**M1 — expander pass.** `include/hsc/surface/expand.hh` +
`src/surface_expand.cc`; datum writer in the syntax layer; `expand` wired
between parse and translate in `run_file` and `hsc-mcc`; `--expand` dump in
the `hsc` runner. Gate: existing samples pass unchanged (the pass is the
identity on binder-free input).

**M2 — parametric samples, each a self-checking oracle** (`samples/param/`):

1. `hanoi.hsc` — `param N/P`, count-form array, nested `exists` family with
   dependent-range `forall` guard, shape by `forall`. Oracle: `(expect 3^N)`,
   independently `tools/hanoi.py N` produces the same count — two generators,
   one answer.
2. `philo.hsc` — classic N philosophers as two arrays (state, fork), a
   3-event family with `(% (+ i 1) N)` fork addressing. Oracle: saturate vs
   `naive` reach agree (differential), count recorded in the report.
3. `rotate.hsc` — `init` by `forall` (cell i := i), one simultaneous-clause
   rotation event. Oracle: `(expect N)` (the N cyclic shifts).

**M3 — the layout experiment** (the libITS scalar-set lesson: grouping is
where the payoff lives). Same philosophers model at `spine` vs `balanced`
vs grain-blocked shapes; report nodes/peak/time per layout. This drives
whether a block-layout sort sugar is worth adding.

## 5. Expected properties, checkable

- Expansion is the identity on binder-free files (byte-stable modulo
  whitespace under `--expand`).
- `--expand` output is itself a valid `.hsc` file whose run is
  observationally identical to running the parametric file.
- Errors carry the source line of the offending form; shadowing, name
  collisions, empty events, exists-in-clause misuse are refused loudly with
  the fix named in the message.
