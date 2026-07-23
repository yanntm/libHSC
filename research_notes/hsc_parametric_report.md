# Parametric surface — engineering report

Response to `hsc_parametric.md`. All data reproducible from the named
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
- **The operation side is Θ(N) by construction, twice over.** (1) The
  expander unrolls: 3 templates become 3N flat events — a 2^100-instance
  expansion is text, impossible. (2) The compiled terms do not share
  across instances: op terms sit at 6 per philosopher exactly (98304 + ε
  at N=16384), because every term addresses *absolute* frontier
  positions — `take1_17` and `take1_18` are distinct interned codes
  differing only by a translation, and interning cannot see conjugacy.
  Note the seed of the fix already exists: `read_when` shifts a
  single-leaf guard to position 0 before interning, which is why the
  guard *bexprs* are shared; the products and case brackets above them
  are not.
- **Wall time is superlinear on top** (×4 N → ×12 time net of the
  expander), pointing at a per-event cost that grows with the frontier
  (position vectors / shape descent per term). Worth a profile before
  naming the exponent; irrelevant to the asymptotic goal, which the
  previous point caps at Θ(N) regardless.

### What O(n) for 2^n needs (the SDD construction, in our terms)

The sort side is ready: `shape_table::pair` interns, so the balanced sort
over 2^n leaves is n nodes — a DAG, already. Missing, in order:

1. **Translation-invariant event terms**: terms addressed relative to a
   subtree root, so one code serves every block at its level.
2. **A repetition/induction combinator over the sort DAG**: the events of
   a block of 2^(k+1) defined from the events of its two half-blocks plus
   an O(1) seam term (fork shared across the cut), 3 templates × n levels
   = O(n) codes; the ring-closing seam once at the top.
3. **Log-form cardinality** (the double is exhausted at 1e15).

Then saturation's existing caches close the loop: identical sub-block ×
identical (now shared) operation = one computation. This is the
orbit/symmetry thread of the spec's §3, promoted from "paper-sized,
later" to *measured and motivated*: the substrate is provably ready, the
term algebra is the gap.
