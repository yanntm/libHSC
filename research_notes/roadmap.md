# libHSC roadmap

*Milestones are features: each one names what you can do after it that you
could not do before, demonstrated at the surface once the surface exists
(M3+). Engineering process (specs, reports, tests) follows the house
convention and is not restated per milestone. Supersedes §7 of
`ddd_to_hsc.md`.*

## Relation to legacy code, once and for all

Four modes, and nothing else:

- **Reference.** `~/git/libDDD` and `~/git/libITS` are read to understand
  semantics and pitfalls. Never linked, never wrapped, never built into
  libHSC.
- **Re-expression.** Algorithms whose *logic* we keep are rewritten
  against the new interfaces, from the bridge document's correspondence
  table: the SDED union as Construction 3.3, the F/L/G schedule, the
  ExprHom query semantics. Code is written new; the legacy source is the
  spec.
- **Port.** Exactly one component is ported as a design with its code
  adapted: the flat-engine storage (trailing-array nodes, id-based unique
  table) as the internals of the fast leaf theory — at M6, not before.
- **Baseline.** Legacy tools run as external binaries for benchmark
  comparison. They are competitors, not dependencies.

Decisions already burned in (see `ddd_to_hsc.md` §5 and discussion):
positional variable naming by unconditional application of the declared
order, indexed bottom-up so codes align with suffix sharing; zero as
absence; id-coded interning with generation tags from the start (cheap at
M1, prerequisite for M7); no `top`, no `invert`, no MLHom.

---

## M1 — The interfaces

**Feature: the API *is* the calculus, reviewable as such.**

Headers defining the structural elements, compilable, minimally stubbed:

- Shapes (`1 | ⟨A⟩ | (V_h,V_t)`), positions as paths.
- The code/interning layer: one mechanism, id-coded, generation-tagged.
- The leaf-theory concept: tiers E/J/G, `split_equiv`, `apply_local`
  (whole terms), exported maps — as C++20 concepts with contracts stated
  in comments.
- Diagram and operation-term declarations (basis: `id`, `local`,
  `query;case`, `∘`, `+`, `∗` — `∗` declared, inert until M5).

Deliverable is a design review artifact: reading the headers against
`hsc_core4.md` should be a §-by-§ correspondence.

## M2 — Hierarchical integer sets

**Feature: build and combine hierarchical sets of integer tuples.**

The simplest leaf theory instantiated: finite integer sets (the
IntDataSet of the new world — plain sorted values over small domains,
enumeration-honest). Over it:

- The diagram type with Construction 3.3 as its one canonicalizer
  (re-expressed from the SDED union).
- Internalisation exercised from the first line: diagrams as leaf codes,
  shapes with composite heads — hierarchy is never a later feature.
- Union, intersection, relative difference, emptiness, equality (pointer
  test), state counting, size (per-node prime counts), printing.

What you can do: construct the philosophers *statics* — the state space
as data, hierarchically — and watch it stay linear in `n`. No operations
yet beyond the set algebra.

## M3 — The surface

**Feature: write examples and reason without writing C++.**

The SMT-flavored s-expression language and an interactive mode:

- `(declare-leaf ...)`, `(declare-shape ...)`; building sets by
  assertion; the set algebra of M2 as commands.
- `(size X)`, `(print X)`, states counts; a first `(bill)` with whatever
  meters exist.
- File loading + REPL; an `examples/` folder that every later milestone
  extends.

From here on, a milestone is done when its feature works *at the
surface* with a committed example.

## M4 — Operations, up to but not including fixpoint

**Feature: fire transitions on hierarchical states, including
arithmetic across levels.**

- Local terms: guards and assigns whose support sits in one leaf, handed
  whole to the theory (`:when`/`:do` surface form; the compiler decides
  local-vs-crossing from where positions sit — the modeller is not
  told).
- Combinators: `∘`, `+`, structural maps; guard negation at tier G
  (deadlock predicates work).
- The query/case bracket, and with it **cross-level arithmetic**: the
  LIA interchange theory — expressions, currying, residuals — giving
  crossing guards (`a < b + c` across the shape) and crossing assigns
  (`x ≔ y + z`, `tab[i] ≔ e`). This is the ExprHom semantics
  re-expressed over hierarchy: the generalization that never happened in
  legacy, landed as a feature. Kernels may start naive (merge by
  identical residual only); federation completes in M8.

What you can do: one-step successor and predicate evaluation on
hierarchical states, philosophers events firing, indirection examples
(`tab[tab[x]]`) resolving.

## M5 — Star and saturation

**Feature: reachability.**

- `∗` activated; the F/L/G schedule with chaining, hierarchical by
  construction (§6 of the calculus: the split derived from reach,
  nothing declared).
- Fused re-saturation in crossing replies; the trivial-case regime
  recognized (events that factor never build a case).
- Congruence-tower inspection on results.

What you can do: `(reach)` philosophers end-to-end at the surface;
correctness against brute enumeration on small instances; first honest
look at the bill on growing `n`.

## M6 — The fast leaf

**Feature: speed — the parity gate.**

- Port of the legacy flat-engine storage as the bounded-integer/FDD
  theory: trailing-array nodes, id unique table with transparent lookup
  and generations; the declared-regime path monomorphized so the general
  mechanism costs the special case nothing.
- Benchmark harness: legacy libDDD/ITS-tools as external baselines on
  P/T saturation (philosophers, standard Petri corpus).

Gate, stated plainly: parity with legacy on the declared regime, or the
interface work goes back to M1 with the measurements in hand. This is
where "the general mechanism is never a tax" is proven or falsified.

## M7 — Policies and the invoice

**Feature: the bill, and caches you can govern.**

- The invoice counters of the calculus (§12.5): residuals per query,
  levels crossed, leaf calls, meeting pairs, certificates
  held/dropped/recomputed.
- Retention policies as declared, pluggable objects over those meters;
  GC with attributable invalidation (the generations of M1 earning their
  keep). No cache sacrosanct — saturation memos are staleness bets like
  any other; wipe-all remains a legal policy, now one point in a space
  instead of the only point.

## M8 — Breadth

**Feature: more theories, more front ends, the paper.**

- Kernel/labelling federation completed (retroactive merge).
- A GAL importer (reusing the legacy parser as reference), so existing
  models run.
- An ω-word / recognizable-language leaf theory (from the aut2ltl work).
- Disposition selection informed by the invoice (join-selection rule).
- The paper's experimental section generated from the invoice; drafts
  revised against what implementation taught.

Order within M8 is negotiable; entry into M8 is not — it requires M6's
gate passed.
