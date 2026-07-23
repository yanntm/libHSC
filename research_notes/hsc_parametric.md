# Parametric surface — spec

Binders and compile-time parameters for the `.hsc` surface: express a family
of N identical components once, as one template. Design settled in discussion;
this file is the engineering contract. Companion report:
`hsc_parametric_report.md`.

Part I (§0–5, milestones M1–M3): the expander — done, see the report.
Part II (§6–11, milestones M4–M7): symmetric encodings, the declared route —
no enumeration anywhere in the pipeline for a certified family.

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
  pattern subset is a symmetry certificate (Murphi / libITS lineage). The
  *checker* and the fold it licenses are now Part II (§6–11); **orbit tags /
  quotient reduction** remain deferred — Part II shrinks the representation,
  not the state space.
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

---

# Part II — symmetric encodings: the declared route

Part I makes a family cheap to *write*; the M3 report shows what it still
costs to *build*. The diagram side is already O(log N) (interning), the
operations already share across instances (currification + re-rooting), and
`sum_at` folds the wrapper chains after the fact — but the chains are still
built, the expander still unrolls text, the translator still walks N
instances. The only Θ(N) left is enumeration itself. Part II removes it:
a binder family that satisfies a checkable uniformity certificate is
**never expanded**. Arrays, shape, init, event terms and quantified criteria
all build by recursion over the interned sort DAG — O(log N) time and codes
for the whole system. This is the "declared" route of the report's
diagnosis, the working half of the paper's Open 6.

Scope, honestly: this is the **fold, not the quotient**. No orbit reduction
happens; the state space keeps its full size, and only its representation
shrinks. The certificate is per family: a model may mix certified families
(folded) with arbitrary flat events (enumerated below the bound); the system
term is their sum. Declared and enumerated denote the same system — Prop 4.5
plus `sum_at` make that checkable by *code equality* at small N, which is
the primary gate throughout.

## 6. The certificate

A clause-level `(exists (i N) BODY)` (event family), a `(forall (i N) BEXP)`
guard or query criterion, or a `(forall (i N) …)` init clause, indexing
count-form arrays of count N, is **uniform** when:

- **C1 — index discipline.** Every occurrence of `i` in BODY is in index
  position of an access `(at a IDX)` with `a` a count-N array and IDX one
  of `i`, `(% (+ i k) N)`, `(% (+ i (- N k)) N)`, `k` a literal —
  normalized to a signed offset δ; Δ ⊆ [−K, +K] is the family's offset
  set, K a small constant. `i` occurs nowhere else: not as a value, not
  compared to anything, not in another binder's bounds.
- **C2 — uniform template.** No other occurrence of `N` in BODY (outside
  the mod patterns); no grounded access to the family's arrays inside
  BODY. Accesses to leaves outside the family's arrays (globals) are
  allowed and must not mention `i`.
- **C3 — full range.** The binder range is exactly [0, N).
- **C4 — aligned layout.** The shape clause presents the family's cells as
  `(balanced (forall (i N) CELL+))`: components in index order, each
  component's cells contiguous. The per-i cell list defines the component
  sort C; the family block sort is the balanced pairing over components.
- **C5 — one index.** A single binder. Multi-index families (hanoi's
  `move`) are not certified — their orbit structure is richer than
  translation; they expand as Part I.

Two certified classes: **scalar** — Δ = {0}, no cross-component access,
with or without globals (mutex: scalar-with-global); **circular** —
Δ ⊆ [−K, K], K ≥ 1 (philo: K = 1). rotate's event is circular but its
init violates C1 (`cell i := i` uses `i` as a value) — correctly so: its
diagram is Θ(N) whatever the encoding, and the certificate's job is to
refuse to promise otherwise.

Refusal is loud, and enumeration is bounded: if the certificate fails and
expansion would exceed `--expand-bound` (default 2^16 instances), the model
is refused with the violated clause and the offending form's line — never a
silent Θ(N) attempt. Below the bound, uncertified binders expand as Part I.
`--family=unfold` forces enumeration of a certified family (for the
differential gates); `--expand` keeps its Part I meaning (bounded, textual).

## 7. The construction

One discipline throughout: recursion over the interned sort DAG, memoized
per sort node — never a loop over instances.

- **Sorts.** `B(1) = C`; `B(n) = (B(⌈n/2⌉), B(⌊n/2⌋))`. Interning
  (`shape_table::pair`) makes this O(log N) distinct sort nodes for any N,
  power of two or not.
- **Symbolic N.** The point is N = 2^100, which exceeds machine integers:
  `param` arithmetic and grounded-index folding go arbitrary-precision
  (the same bignum §9 needs — one mechanism, two uses). Cells are never
  named: at symbolic scale the count-form array declares no `a_i` atoms;
  a grounded `(at a k)` resolves to a position path by arithmetic on `k`
  (its binary digits are the path through the balanced pairing).
- **Templates.** Each family event `e` has support offsets Δ_e (shifted so
  min Δ_e = 0). Events with Δ_e = {0} are local: their currified terms sum
  into the base template `T ∈ 𝒯(C)`. Events with δmax ≥ 1 are seam
  templates: for K = 1, a term over `(C, C)` — left part conjugated to the
  rightmost component of a block, right part to the leftmost component of
  the next. The conjugation wrappers are the spine chains
  `node(id, ·)^depth` and `node(·, id)^depth`, each level's chain being
  the previous plus one wrap — O(1) new codes per level, shared across
  levels by interning.
- **The recurrence.**

  ```
  R(B(1)) = T
  R(B(n)) = R(h) ⊗ id  +  id ⊗ R(t)  +  Seam(h, t)
  System  = R(B(N)) + Ring
  ```

  `Seam(h,t)` carries the instances crossing the top cut of the block
  (δmax instances per crossing template; K = 1 gives one); instances
  interior to a half are inside `R(h)`/`R(t)` by recursion. `Ring` is the
  wrap-around instances (i ≥ N − δ) conjugated to (rightmost, leftmost)
  of `B(N)` — the ring closes once, at the root. With a global touched
  uniformly by every instance (mutex), the sum common-factors:
  `Σ_i (g ⊗ w_i) = g ⊗ R` — the common-factor generalization of
  Prop 4.5 (equal heads merge by summing sections; one-line proof, for
  the paper when this lands).
- **Init.** A certified uniform init (constant body) builds the initial
  word by the same recursion: `word(B(n)) = (word(h), word(t))`, O(log N)
  nodes. Non-uniform init refuses at symbolic scale (rotate, honestly).
- **Quantified criteria fold too.** A certified `(forall (i N) BEXP)` with
  δmax = 0 is a product term built by the same recursion (Cor 6.3 at
  O(log N) codes); δmax = 1 (adjacent-pair invariants — philo's
  neighbour-clash check) is the same recurrence with filter seams. The
  properties we check at 2^100 build the same way the system does.
- **Grammar surviving expansion.** The expander checks the certificate and
  emits, instead of instances, a translator-consumed form

  ```
  (family NAME (i N) BODY)    ; offsets normalized, certificate attached
  ```

  Exact spelling of the offset annotation is engineering's choice; the
  form must be dumpable and re-readable.
- **Naming.** A family keeps one event name; witnesses print `NAME[i]`
  with `i` recovered from the position path. Word literals at symbolic
  scale print run-compressed (uniform blocks once, with a count). Full
  witness support at symbolic scale may lag M7 — count and select drive
  the headline.

## 8. The schedule consumes the fold

A folded term arrives at the saturation split pre-partitioned at every
level: at block sort `B(n)`, `F = id ⊗ R(t)`, `L = R(h) ⊗ id`,
`G = Seam(h,t)` (+ `Ring` at the root) — Theorem 5.2 read directly off the
form. The engine must consume this without flattening: today the partition
flattens `alt`/sum summand lists per position; on a folded operand it must
read the three groups off the head-folded form as-is.

Fixpoint memoization is already keyed by (term code, node code). With
folded terms and a shared diagram, those keys coincide across all blocks of
a level: the closure at each level is computed once, O(log N) distinct
fixpoint computations for the whole reach — the classic hierarchical-SDD
result, reconstructed. That count is the measurement: **instrument the
fixpoint cache** (distinct closure keys per run) and report it. If sharing
degrades — schedule-order divergence of intermediate block contents — the
counter localizes it; that is a finding for Theory, not a silent slowdown.

## 9. Cardinality: exact and log-form

`cardinal` as a double is exhausted at ~1e15 (M3: cross-layout counts
disagree in low digits at 1e97). Two forms replace it:

- **Exact.** Per-node memoized big-integer count: leaf codes count exactly
  from interval widths; `|node| = Σ |P_i| · |S_i|`. A minimal bignum in
  `util/` (2^64 limbs, schoolbook), no external dependency — shared with
  §7's symbolic-N arithmetic. Practical regime: up to ~10^4 printed
  digits (philo to N ≈ 2^14), trivially fast at these diagram sizes.
- **Log-form.** Per-node log2 as double via log-sum-exp; reported as
  `≈ 10^X`, relative error ~1e−15 *on the log*, always marked
  approximate, never presented as exact.

`(count)` prints exact when feasible, log-form otherwise; flags force
either.

**Oracles** — closed forms independent of both engines:

- philo ring: `count(N) = a_N`, `a_1 = 2, a_2 = 6,
  a_N = 2·a_{N−1} + a_{N−2}` (= `(1+√2)^N + (1−√2)^N`). Derived by Theory
  from the two report points — a_5 = 82 (M2) and
  a_256 = 9.7853e97 (M3, log10 = 256 × 0.3827757 = 97.9906) — so
  **verify at N = 3..8 against both engines before trusting it upward**.
  Exact regime: bignum matrix power; log regime:
  `log2 count = N × 1.2715533…` (correction term < 1e−300 at large N,
  ignored). Small table: 14, 34, 82, 198, 478, 1154 for N = 3..8.
- mutex: `count(N) = 3^N + 1` (basis: 82 at N = 4, verified by hand in
  M2). `log2 count = N × log2 3 = N × 1.5849625…`.

## 10. Milestones

**M4 — symbolic pipeline + the declared fold.** Certificate checker in the
expander (emits `family`, unexpanded); symbolic arrays/shape/init (no cell
atoms, positions by arithmetic, bignum param fold); translator builds `R`
by recursion over the sort DAG. Gates:

1. **Code equality**: declared system-term code == enumerated +
   `sum_at`-folded code, philo and mutex, N ∈ {4, 8, 16, 64}. If codes
   differ but reach agrees, that is the fallback gate — report *why* they
   differ (summand ordering? seam placement?) before proceeding.
2. **Build cost linear in log N**: measure k = 8..20 on philo (one
   recurrence, all three event templates in one T). Expect op-term codes
   ≈ 30 + c·k with c ≲ 15, vs 6N enumerated (98335 at k = 14); build
   time milliseconds, flat in N.

**M5 — the schedule.** Partition reads the folded form; closure-key
instrumentation (§8). Gates:

1. Reach on uniform-init philo, k = 8..20: counts agree with the §9
   oracle; R nodes = 4k (M3 extrapolation — 32 at k = 8 and 56 at k = 14
   are both on that line); distinct closure keys O(k), reported.
2. Wall time polynomial in k — seconds at k = 20; report the measured
   slope. (This instrumentation may also localize Part I's standing
   superlinear-cost item for free.)

**M6 — cardinality.** §9 built. Gates: philo recurrence verified N = 3..8
on both engines; exact equality to a_N for sampled N ≤ 2^12; the three M3
layouts at N = 256 now agree *exactly*; mutex 3^N + 1.

**M7 — the headline.** philo at N = 2^k, k ∈ {20, 40, 60, 80, 100}:
report R nodes (predict 4k = 400 at k = 100), op-term codes (predict
≤ 2000), distinct closures, wall time, log-form count vs oracle
(log10 ≈ 2^100 × 0.3827757 ≈ 4.852e29 — a state space of 10^(4.9·10^29)
states). Plus one adjacent-pair invariant checked at 2^100 (M2 philo's
neighbour-clash criterion, folded per §7): the claim is querying at that
scale, not only counting.

## 11. Expected properties, checkable

- The declared route never enumerates: no O(N) loop in the pipeline for a
  certified model — code inspection plus the flat-in-N build-time gate.
- Declared ≡ enumerated: code equality at small N (primary), differential
  reach (fallback, with the discrepancy explained).
- Certificate refusals name the violated clause and the source line;
  above-bound uncertified expansion is refused, never attempted.
- Every number in the report traces to a closed-form oracle or an exact
  bignum count; log-form figures are marked approximate everywhere they
  appear.
