# libHSC roadmap

*Milestones are features: each one names what you can do after it that you
could not do before, demonstrated at the surface once the surface exists
(M3+). Engineering process (specs, reports, tests) follows the house
convention and is not restated per milestone. Supersedes §7 of
`ddd_to_hsc.md`.*

## Relation to legacy code, once and for all

Five modes, and nothing else:

- **Reference.** `~/git/libDDD` and `~/git/libITS` are read to understand
  semantics and pitfalls. Never linked, never wrapped, never built into
  libHSC.
- **Re-expression.** Algorithms whose *logic* we keep are rewritten
  against the new interfaces, from the bridge document's correspondence
  table: the SDED union as Construction 3.3, the F/L/G schedule, the
  ExprHom query semantics. Code is written new; the legacy source is the
  spec.
- **Adapt (libsdd).** `~/git/libsdd` (Hamez, BSD-2) is the third
  reference base (see `ddd_to_hsc.md` §8); selected files are adapted
  into GPL variants with attribution kept — Alexandre Hamez is cited in
  the adapted headers and in the README. This starts at M1 (the cache
  and its intrusive table), not M6.
- **Port.** The legacy flat-engine storage (trailing-array nodes,
  single-TU hot loop) as the internals of the fast leaf theory — at M6,
  not before.
- **Baseline.** Legacy tools and libsdd run as external binaries for
  benchmark comparison. They are competitors, not dependencies.

**Dependencies policy.** C++23, CMake. External deps: **google
sparsehash** (github.com/sparsehash/sparsehash — maintained, std-only,
fetched not vendored). It is kept on merit, not nostalgia: sparse
hashing is a genuinely different algorithm (bitmapped groups, ~2 bits
overhead per entry) that measured 30–40% less memory than
`unordered_map`-family tables on DD workloads, whose dense-buckets
assumptions have not fundamentally changed; `sparsetable` also serves
the sparse refcount array. **No boost** — where libsdd uses
`boost::container::flat_set`, C++23 `std::flat_set` or a sorted vector
replaces it. Everything else is std.

Decisions already burned in (see `ddd_to_hsc.md` §5 and discussion):
positional variable naming by unconditional application of the declared
order, indexed bottom-up so codes align with suffix sharing; zero as
absence; id-coded interning with generation tags from the start (cheap at
M1, prerequisite for M7); no `top`, no `invert`, no MLHom.

---

## M1 — Infrastructure and interfaces

**Feature: a measured hash-consing and caching substrate, plus headers
that read as the calculus.** Hashing and caching are *the* cross-cutting
concern of a DD library; M1 gets them right once, as comfortable
infrastructure, before anything is built on top.

### Source organization (created at M1)

```
include/hsc/
  util/      hash primitives, small utilities
  mem/       interning + caches — the M1 substrate
  core/      shapes, theory concepts, diagram declarations
  leaves/    leaf theories (M2+; empty at M1)
  ops/       operation terms (M4+; declarations only)
src/         non-header implementation
tests/       correctness (differential where possible)
bench/       measurements; every M1 claim gets a number here
examples/    (M3+, surface examples)
```

Mirrors the Python prototype's layering (core / classify / ops /
leaves); `mem/` and `util/` sit below everything; nothing in `mem/`
knows about diagrams. No singletons anywhere: tables and caches are
owned by a `manager` object and threaded as explicit contexts (libsdd's
design — `sdd/manager.hh`, `sdd/hom/context.hh`).

### Modules, with provenance

- `util/hash.hh` — the `hash()` customization point; integer mixers
  (from legacy `ddd/hashfunc.hh`: wang32 et al.); **non-commutative
  combine** as the only sanctioned way to hash composites (kills the
  legacy XOR-pair collision, `hash_support.hh:96`); hashing style per
  libsdd `sdd/util/hash.hh` (seed/combine visitors).

- `mem/intern.hh` — `intern<T, Id = uint32_t>`, *the* unique table, one
  mechanism for nodes, terms, kernels alike. Design from legacy
  `ddd/UniqueTableId.hh` — id handles, index vector id→object, free
  list, **sparse refcounts** (`google::sparsetable`) — with the known
  fixes applied from day one: heterogeneous/transparent probe lookup
  (no tmpid slot, no temp allocation on the hit path), explicit free
  list (no pointer punning), iterative marking hook, swap-not-copy on
  rebuild, and **generation tags** (bumped on id reuse; checked by
  certificate holders, never on the hot handle path). Backing store:
  `sparse_hash_set` of ids. The insert path adopts libsdd's
  **allocation cache** (`sdd/mem/unique_table.hh::allocate` — the table
  keeps the largest recently-freed block to serve the next miss).
  Ids are 32-bit by default and stay shorter than pointers — 64 bits
  per edge is too expensive, and at 4G nodes we are dead in the water
  anyway — but `Id` is a parameter, the system is not hostile to wider
  ids.

- `mem/handle.hh` — `weak<T>` (a raw id: trivially copyable, zero RC
  churn on the hot path) and `strong<T>` (refcounted through the
  table). The legacy GDDD/DDD split, kept because it is right; libsdd's
  per-copy RC churn is what we are avoiding.

- `mem/cache.hh` + `mem/pool.hh` — the bounded operation cache: GPL
  variant of libsdd `sdd/mem/cache.hh`, `cache_entry.hh`, `lru_list.hh`
  and the intrusive no-rehash `sdd/mem/hash_table.hh` (attribution
  kept). Fixed capacity, all memory at construction, pool-allocated
  entries, operation-as-key (the op object holds its operands), LRU
  eviction — with three deltas: a **batch-eviction knob** (evict K, not
  1 — the evict-one-at-capacity steady state thrashes), runtime filters
  alongside the compile-time chain, and the stats struct extended
  toward the invoice.

- `mem/stats.hh` — statistics structs for every table and cache
  (libsdd's `cache_statistics`/`unique_table_statistics`, extended).
  The invoice starts at M1: nothing is ever added to `mem/` without its
  meter.

- `core/shape.hh`, `core/theory.hh`, `core/diagram.hh` — the calculus
  interfaces: shapes (`1 | ⟨A⟩ | (V_h,V_t)`) as interned trees with
  positions as paths; the leaf-theory concepts (tiers E/J/G,
  `split_equiv`, `apply_local`, exported maps) with contracts in
  comments; diagram and operation-term declarations (`id`, `local`,
  `query;case`, `∘`, `+`, `∗` — `∗` inert until M5). Reading these
  against `hsc_core4.md` should be a §-by-§ correspondence.

### Hygiene rules (set once, at M1, enforced thereafter)

1. Everything interned defines `hash()` and `==`; hash memoization is a
   per-type decision, never an infra assumption.
2. Composite hashes use the non-commutative combine; XOR of hashes is
   banned.
3. No singletons; no global state outside the manager.
4. Every table/cache exports its stats struct; `(bill)` will read them.
5. Probe by view, allocate only on miss.
6. An id is never silently reused while cited: generation bump on free,
   `valid(id, gen)` as the check.
7. New interned kinds are instantiations of `intern<T>`, never parallel
   mechanisms.

### Deliverable

Compiling library + `bench/` numbers: interning throughput and resident
memory vs `std::unordered_set` on node-shaped payloads (the sparsehash
memory claim, re-measured on our workload, in a committed report), and
cache hit/eviction behavior on a memoized recursion. The feature is the
substrate *with its numbers* — M2 builds on it without revisiting it.

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
