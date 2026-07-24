# Parametric surface — engineering report

Results of the parametric-surface work (its spec, `hsc_parametric.md`, is
retired to git history — all milestones met). All data reproducible from the named
git-tracked files with `hscrun` (`examples/hsc.cc`); every sample is
self-checking (`expect`, exit code).

## M1 — the expander: done

`include/hsc/surface/expand.hh` + `src/surface_expand.cc` (~600 LOC), wired
into `run_file`; `hscrun --expand FILE` dumps the lowering. `hsc-mcc` was
left unwired on purpose: its forms come from the Petri front end, generated
and binder-free. Gates held:

- Identity on binder-free input: `--expand samples/divine/hsc/at.1.hsc`
  reproduces the file modulo whitespace; the full doctest suite (25926
  assertions) and spot-checked divine samples pass unchanged.
- `--expand` output of every parametric sample is itself a runnable `.hsc`
  with identical results (checked on hanoi: 63 = N·P·P events, the a==b
  degenerates present as text and folded to zero terms by the translator).

Deviation from spec, favorable: empty-`exists` wrap positions emit
`(abort)`, a new base-grammar EVTERM meaning the zero term
(`surface/algorithm.md` grammar updated) — replacing the planned
`(when (!= 0 0))` spelling.

## M2 — samples: done (`samples/param/`, see its README)

| sample | N | count | oracle |
|---|---|---|---|
| hanoi.hsc | 7 rings, 3 poles | 2187 | `= 3^7`; `tools/hanoi.py 7` agrees (independent generator); goal select = 1 |
| philo.hsc | 5 | 82 | saturate ≡ naive; neighbour-clash image empty |
| mutex.hsc | 4 | 82 | saturate ≡ naive; = 3^4 + 1 by hand; mutual-exclusion image empty |
| rotate.hsc | 6 | 6 | = the N cyclic shifts (indexed init + simultaneous rotation) |

The mutex count 82 is verified by hand: 3^4 = 81 free interleavings plus
the single all-frozen critical-section state. Note the GAL original's
bootstrap pseudo-state *aliases* that section state, so its count is not
inflated — the init event's advantage is stating the intended start
directly, not a smaller state space. The GAL references live in
`~/git/ITS-Exercise` (read-only).

## Findings for Theory

1. **`select` vs the doc's grammar.** `algorithm.md` reads
   `QATOM ::= BEXP`, but `do_select` accepts only single-leaf atoms and
   two-leaf comparisons; a quantified disjunction across leaves
   (`(exists (i N) (and …))`) is refused ("expected a symbol"). The
   canonical workaround — a `(when …)` filter applied with `apply` — works
   and is used in philo/mutex; it lands in the §7 case engine. Spec §3 now
   carries the seam; either the doc narrows or `select` routes rich BEXPs
   through the same path.
2. **Suffix-naming of families** (`move_2_0_1`) is user-visible in traces
   and witnesses. Fine for now; worth a thought when witnesses cite events.

## M3 — layout experiment: run, and it diagnoses the road to 2^n

Files: `samples/param/layout/philo_{spine,balanced,blocked}.hsc` — the
identical N=256 model at three shapes (blocked: 16 blocks of 16).

| layout | R nodes | total nodes | op terms |
|---|---|---|---|
| spine | 2548 | 7124 | 3579 |
| balanced | 32 | 185 | 1555 |
| blocked 16×16 | 216 | 785 | 1035 |

All three report count ≈ 9.785e97 — agreeing only to ~15 significant
digits, because `cardinal` is a double: at this scale the printed counts
differ in the low digits across layouts. **Finding: exact (or log-form)
cardinality is needed before any cross-check beyond ~1e15 states.**

Scaling the balanced variant (same file, N raised):

| N | R nodes | op terms | wall time |
|---|---|---|---|
| 256 | 32 | 1555 | ~0 |
| 512 | 36 | 3093 | 0.06 s |
| 1024 | 40 | 6167 | 0.06 s |
| 4096 | 48 | 24603 | 0.30 s |
| 16384 | 56 | 98335 | 3.56 s (expander alone: 0.23 s) |

### Diagnosis: why this is not O(n) for 2^n philosophers

- **The diagram side already is O(n).** R nodes grow by exactly +4 per
  doubling of N: the substrate's interning collapses identical balanced
  sub-blocks, as designed. Extrapolated, the reachable set of 2^100
  philosophers is a few hundred nodes.
- **The operation side is Θ(N) — but for a sharper reason than first
  written here.** The residual operations *do* share across instances:
  the engine re-roots on descent (`make_event` recurses with
  `shift_positions`; apply's residuals are rebuilt interned), so the
  term philo 14/15 sees at its subtree is the *same code* philo 18/19
  sees. Verified by a single-family probe (take1 only, balanced): op
  terms are 2079 at N=1024 and 4130 at N=2048 — that is 2N + ~30, where
  the ~30 N-independent codes are the shared operations and the 2N is
  wrapper chains: each instance still needs its path of
  `node(·,id)` / `node(id,·)` wrappers saying *where*, and the distinct
  wrapper codes over all N paths number exactly 2N−1 per family (one
  per subtree×offset pair; 3 families ≈ 6N, matching 98335 vs predicted
  98301 at N=16384 to within 34). The Θ(N) is pure **site
  enumeration**, not a failure of term sharing. (And the expander
  unrolls: 3 templates become 3N events — a 2^100 expansion is text,
  impossible, regardless.)
- **Wall time is superlinear on top** (×4 N → ×12 time net of the
  expander), pointing at a per-event cost that grows with the frontier
  (position vectors / shape descent per term). Worth a profile before
  naming the exponent; irrelevant to the asymptotic goal, which the
  previous point caps at Θ(N) regardless.

### What O(n) for 2^n needs (the SDD construction, in our terms)

The sort side is ready (`shape_table::pair` interns: the balanced sort
over 2^n leaves is n nodes), and translation invariance of the
operations is *already delivered* by the re-rooting above. What remains
is collapsing the site enumeration, and it is a factorization, not new
semantics. By distributivity of sum over the head application,

    node(A,id) ⊕ node(B,id)  ≡  node(A⊕B, id)

so the N wrapper chains around one shared term T fold into the
recurrence `R(d) = node(R(d−1), id) ⊕ node(id, R(d−1))` with `R(0) = T`
— **O(log N) codes for the whole family**. Seam events (fork shared
across a cut) add an O(1) boundary term per level — exactly the
circular-set CUR/NEXT shape — and the ring closes once at the root.
Two routes to it:

1. **Discovered** — **implemented**: `core::sum_at(mgr, sort, events)`,
   the head-folded sum. It could not be a blind `op_table::sum` normal
   form: a node over a leaf head holds *theory* terms, whose sum is the
   theory's `term_sum`, and op terms do not carry their sort — so the
   fold is a sort-aware recursion mirroring the saturation split (one-
   sided wrappers fold per side; the mixed pair and crossing terms stay
   flat summands). The saturation partition now flattens `sum` summands,
   so a folded system schedules identically to its flat list. Wired
   into the naive reach and the `alt` combinator; validated by the
   naive-vs-saturate differentials (philo, mutex, and a take1-only
   probe at N=64: both engines 2^64 exactly), the full doctest suite,
   and the divine sweep. As predicted it changes structure, not time:
   the wrapper chains are still built before they fold.
2. **Declared** — the binder already knows the family is one template
   conjugated over i; the translator builds `R(d)` directly by recursion
   over the interned sort DAG, never enumerating: O(depth) time and
   codes. This is the only route to 2^100, and it is where circular-set
   style constraints (uniform CUR/NEXT access, no symmetry-breaking
   index) become preconditions the expander can check from the binder.

Engineering care, either route: the F/L saturation schedule today
flattens `alt` into a summand list partitioned per position; it must
learn to consume the factored form natively, so the fixpoint at a block
caches by (shared operation, shared block) — identical everywhere,
O(log N) distinct computations. That closing step is the classic
hierarchical-SDD result, reconstructed inside this calculus.

Also needed regardless: **log-form cardinality** (the double is
exhausted at 1e15).
