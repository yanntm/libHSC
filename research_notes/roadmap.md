# libHSC roadmap

*Objectives are features with explicit gates: each names what you can do
after it that you could not before. Revision 2, written from the clearing —
the first version's milestone order was broken twice by contact with the
code, and the reasons are recorded below rather than quietly patched.
Supersedes §7 of `ddd_to_hsc.md`. Live hazards are in
[`HAZARDS.md`](../HAZARDS.md), not here.*

## Relation to legacy code, once and for all

Five modes, and nothing else:

- **Reference.** `~/git/libDDD` and `~/git/libITS` are read to understand
  semantics and pitfalls. Never linked, never wrapped, never built into
  libHSC.
- **Re-expression.** Algorithms whose *logic* we keep are rewritten against
  the new interfaces: the SDED union as Construction 3.3, the F/L/G
  schedule, the ExprHom query semantics. Code is written new; the legacy
  source is the spec. *This is the mode that has done all the work so far,
  and reading the legacy source before writing has paid every time.*
- **Adapt (libsdd).** `~/git/libsdd` (Hamez, BSD-2) adapted into GPL
  variants with attribution kept, per `ddd_to_hsc.md` §8.
- **Port.** The legacy flat-engine storage as the internals of the fast leaf
  theory — at the parity gate, not before.
- **Baseline.** Legacy tools and libsdd run as external binaries for
  benchmark comparison. Competitors, not dependencies.

**Dependencies policy.** C++23, CMake. External: **google sparsehash**
(fetched into `deps/`, not vendored; `configure` run at CMake time for
`sparseconfig.h`, header-only thereafter) and **doctest** (tests only). No
boost. Everything else is std.

*Amended by measurement:* sparsehash serves `sparsetable` for the sparse
refcounts, which is where sparse storage genuinely pays — most nodes are
never referenced. It does **not** back the unique table's probe set:
`sparse_hash_set::find` takes only a key, which forces the legacy
sentinel-id hack, and on a bare 32-bit key set the memory difference is
~10%. That table is ours (`mem/id_table.hh`).

---

## Decisions burned in

Carried from revision 1: positional variable naming by unconditional
application of the declared order, indexed bottom-up; zero as absence;
id-coded interning with generation tags from the start; no `top`, no
`invert`, no MLHom.

Added by revision 2, and the most consequential thing learned so far:

**Operation terms mirror the shape tree.** `H ::= id | node(H_h, H_t) | ∘ |
Σ | ∗`, where the leaf case is a *theory* term read by the theory that owns
the sort. A term is interpreted by whichever algebra it is handed to,
exactly like a value. This one choice dissolved three separate items of
revision 1:

* **Skip** (§5, §5.2's oracles) is `term == id`. There is no oracle, no
  support set, no `get_range`, no per-variable partition cache. `node(id,id)`
  collapses to `id`, so skip trees build themselves.
* **The saturation split** (Def 6.1) is syntactic: `node(h,t)` is `F` when
  `h == id`, `L` when `t == id`, `G` otherwise. libDDD computes this with a
  partition cache over `skip_variable`; libsdd with a `dynamic_cast` chain.
* **Currification** (§2.6, gap D1) is free: descending into a subtree
  re-roots the term, so isomorphic positions share codes. libDDD keys a
  homomorphism on an absolute variable index and structurally cannot.

Anything that would reintroduce absolute position naming into a term is to
be refused on sight.

**Corollary 3.6 is load-bearing, not decorative.** `apply_local` sits on the
support-algebra interface, so `node(h,t)` recurses through the head and tail
algebras without a case distinction — a leaf theory reading its own terms
and a nested diagram reading operation terms are the same code path.

---

## What is done

| | |
|---|---|
| Substrate | hashing with a non-commutative combiner; `intern<T>` probing by view (a hit allocates and constructs nothing), trailing arrays, iterative mark & sweep, generation tags; weak/strong/certificate handles; a fixed-capacity LRU operation cache, everything intrusive; meters on every table and cache |
| Calculus | shapes; the tier-G support contract (join, meet, relative difference — emptiness and equality are free from interning); the normal form of Thm 3.1; **Construction 3.3 as the one canonicalizer**; the set algebra over it; internalisation exercised from the first line |
| Operations | `id`, `node`, `∘`, `Σ`, naive `∗`, and **the F/L/G saturation schedule** of §6, hierarchical by construction |
| Theories | `int_set`, the enumeration-honest reference: sorted runs, and local terms as guard-then-action plus sum and star |
| Evidence | philosophers as data (balanced 8·log₂n nodes vs flat 2n−2, both exact against `trace(Mⁿ)`); Hanoi reachability (3ⁿ exact, naive vs saturated: 45 603× at n=14, n=24 in milliseconds); 200 random models where `saturate == naive fixpoint` |

**How the first ordering was wrong**, kept because it is the useful part:

1. The surface was placed before operations. A surface over data alone can
   only express what already existed. Operations are prerequisite to a
   surface worth writing.
2. `query;case` was bundled with local terms and LIA into one milestone.
   They separate cleanly: Hanoi, Petri nets and philosophers-as-Petri need
   only `local + ∘ + Σ + ∗`. The query bracket is needed for cross-level
   *arithmetic* and nothing before it — so the hardest, most novel part of
   the calculus is off the critical path.

---

## Out of scope: choosing the shape

**Our input is shaped.** NUPN carries a unit tree; the surface's structs and
arrays declare one; a GAL type hierarchy is one. libHSC consumes a declared
shape and refuses to guess.

Deriving a good shape from an unshaped model is a real problem with a known
shape of answer — community detection (Louvain modularity) over a dependency
graph, with control edges distinguished from action edges — and it is a
*specification rewriting* problem that belongs upstream of the library, in
the tooling that produces our input. It is all compromises: Hanoi is best in
blocks of four or five rings, worse as a pure spine, worse again fully
balanced. Nothing about that is a discovery, and nothing about it needs to
live here.

---

## R1 — Models in

**Feature: run a model from a file instead of a `.cc`.**

The gate for everything measurable. Our entire evidence base is two examples
written by the person who wanted them to look good; a memory policy or a
performance claim built on that would be fiction.

- The SMT-flavored s-expression surface: `declare-leaf`, `declare-struct`,
  `declare-shape` with `array` compiling to a balanced tree; events as
  `:when`/`:do`; the set algebra; `reach`; `count`, `size`, `print`, `bill`.
  The expression grammar is written whole and the compiler classifies
  local-vs-crossing, refusing crossing cleanly — `case` is provisioned, not
  implemented.
- Generators in `tools/` emitting `.hsc` files (Hanoi, philosophers, Kanban,
  a Petri family). Loops live in the generator; the reader stays dumb and
  total.
- A **NUPN importer**: places plus a unit tree map directly onto leaves plus
  a shape tree, and it comes with a corpus that has authored hierarchy.
- Bounded integer domains, closing hazard H2 (`shift` under a closure does
  not converge without one).

*Gate: a Petri net from the standard corpus, loaded from a file, reachable
set computed, state count agreeing with a published figure.*

## R2 — The bill, and the memory wall

**Feature: know what a run cost, and survive one that does not fit.**

Demand-driven — R1's first large model decides when. Both halves are one
piece of work.

- `bench/` fills for the first time, on real models.
- The invoice counters of §12.5 over the meters that already exist.
- **Collection actually wired**, resolving H1 and H5 rather than documenting
  them: roots as `strong` handles, cross-table marking, and the operation
  cache either cleared or holding `certificate`s. Retention policy as a
  declared object is the whole point of G5, and `certificate` has been
  written and unused since the first week.

*Gate: a model that exhausts memory under the current unbounded tables
completes under a stated policy, with the invoice showing what was paid.*

## R3 — The parity gate

**Feature: speed. The existential test.**

- The bounded-integer / FDD leaf theory, porting the legacy flat engine's
  storage (trailing-array nodes, the single-TU hot loop).
- Benchmarks against libDDD and ITS-tools as external baselines on P/T
  saturation over the standard corpus.
- The two convergence accelerators deliberately skipped so far, chosen here
  **by measurement** rather than invented: §6.3's fused re-saturation in a
  crossing reply (libDDD's `recFireSat` and the `l&f` nice-form test —
  visible being discovered by hand in `demo/hanoi` v6) and libDDD's DFS/BFS
  knob. Both are convergence heuristics that profile differently per
  workload; neither changes the answer.

*Gate, stated plainly: parity with legacy on the declared regime, or the
interface work goes back to the substrate with the measurements in hand.
This is where "the general mechanism is never a tax" is proven or
falsified.*

## R4 — Queries, cases, crossing arithmetic

**Feature: arithmetic across levels.**

No longer on anyone's critical path, and none the worse for it.

- `split_equiv` as the diagram theory's export — Corollary 3.6's other half.
- The query/case bracket; the LIA interchange theory: expressions,
  currying, residuals. Crossing guards (`a < b + c` across the shape) and
  crossing assigns (`x ≔ y + z`, `tab[i] ≔ e`). This is the ExprHom
  semantics re-expressed over hierarchy — the generalization that never
  happened in legacy.
- Kernel/labelling federation, retroactive merge.

*Gate: an indirection example (`tab[tab[x]]`) resolving at the surface.*

## R5 — Breadth, and the paper

- A GAL importer, so existing models run.
- An ω-word / recognizable-language leaf theory, from the aut2ltl work.
- Disposition selection informed by the invoice.
- The paper's experimental section generated from the invoice; the calculus
  drafts revised against what the implementation taught. Revision 2 of this
  file is already a down payment: §5's skip oracles and §6.1's split both
  became free, and the draft should say so.

*Entry into R5 requires R3's gate passed.*
