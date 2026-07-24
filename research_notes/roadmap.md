# libHSC roadmap

*Objectives are features with explicit gates: each names what you can do
after it that you could not before. Live hazards are in
[`HAZARDS.md`](../HAZARDS.md), not here.*

## Relation to legacy code

Five modes, and nothing else:

- **Reference.** `~/git/libDDD` and `~/git/libITS` are read to understand
  semantics and pitfalls. Never linked, never wrapped, never built into
  libHSC.
- **Re-expression.** Algorithms whose *logic* we keep are rewritten against
  the new interfaces: the SDED union as the canonicalizer, the F/L/G
  schedule, the ExprHom query semantics. Code is written new; the legacy
  source is the spec.
- **Adapt (libsdd).** `~/git/libsdd` (Hamez, BSD-2) adapted into GPL
  variants with attribution kept (see the file headers in `mem/`).
- **Port.** The legacy flat-engine storage as the internals of the fast leaf
  theory — at the parity gate, not before.
- **Baseline.** Legacy tools and libsdd run as external binaries for
  benchmark comparison. Competitors, not dependencies.

**Dependencies policy.** C++23, CMake. External: **google sparsehash**
(fetched into `deps/`, not vendored; `configure` run at CMake time for
`sparseconfig.h`, header-only thereafter), **doctest** (tests only),
**CLI11** (tools only), **expat** (Petri import only). No boost. Everything
else is std. Sparsehash serves `sparsetable` for the sparse refcounts —
most nodes are never referenced. It does not back the unique table's probe
set: that table is ours (`mem/id_table.hh`).

---

## Decisions burned in

Positional variable naming by unconditional application of the declared
order, indexed bottom-up; zero as absence; id-coded interning with
generation tags; no `top`, no `invert`, no MLHom.

**Operation terms mirror the shape tree.** `H ::= id | node(H_h, H_t) | ∘ |
Σ | ∗`, where the leaf case is a *theory* term read by the theory that owns
the sort. A term is interpreted by whichever algebra it is handed to,
exactly like a value. Consequences:

* **Skip** is `term == id`. No oracle, no support set, no per-variable
  partition cache. `node(id,id)` collapses to `id`, so skip trees build
  themselves.
* **The saturation split** is syntactic: `node(h,t)` is `F` when `h == id`,
  `L` when `t == id`, `G` otherwise. libDDD computes this with a partition
  cache over `skip_variable`; libsdd with a `dynamic_cast` chain.
* **Currification** is free: descending into a subtree re-roots the term,
  so isomorphic positions share codes. libDDD keys a homomorphism on an
  absolute variable index and structurally cannot.

Anything that would reintroduce absolute position naming into a term is to
be refused on sight.

**Internalisation is load-bearing, not decorative.** `apply_local` sits on
the support-algebra interface, so `node(h,t)` recurses through the head and
tail algebras without a case distinction — a leaf theory reading its own
terms and a nested diagram reading operation terms are the same code path.

---

## Done (gates passed)

| | |
|---|---|
| Substrate | hashing with a non-commutative combiner; `intern<T>` probing by view, trailing arrays, iterative mark & sweep, generation tags; weak/strong/certificate handles; a fixed-capacity LRU operation cache, everything intrusive; meters on every table and cache |
| Calculus | shapes; the three-map support contract (join, meet, relative difference — emptiness and equality free from interning); the normal form; the one canonicalizer; the set algebra over it; internalisation exercised from the first line |
| Operations | `id`, `node`, `∘`, `Σ`, naive `∗`, and the F/L/G saturation schedule, hierarchical by construction |
| Crossing fragment | the case bracket (`hsc/event.hh`, `hsc/query.hh`): crossing guards, updates and comparisons by `split_equiv` at the cut, curried residuals, interned and cached; `tab[tab[x]]` resolves at the surface |
| Theories | `int_set`, the enumeration-honest reference; `lia`, the interchange theory (expressions, currying, residuals, arrays carrying their placement) |
| Surface | the `.hsc` s-expression language: declarations, events, the event algebra, the state layer, queries; the parametric pass (`param`/`array`/`forall`/`exists`, certified uniform families) |
| Front ends | NUPN/PNML import (`hsc-mcc`, `nupn2hsc`, Louvain decomposition); DVE/BEEM import (`dve2hsc`, 276-model corpus, differential against its-reach) |
| Ordering | FORCE (`hsc/order/`): cliques per event, asymmetric precedences; counts invariant under reordering is a standing regression check |
| Evidence | philosophers balanced vs flat, exact against `trace(Mⁿ)`; Hanoi 3ⁿ exact, saturation 45 603× naive at n=14; 200 random models `saturate == naive`; 168/276 BEEM agree exactly with its-reach |

## Open — performance on BEEM

The frontier is engine cost, not expressiveness: its-reach solves ~50
models we time out on; we solve none it cannot. The work list and next
actions live in `handoff_arithmetic.md`.

## Open — R2: the bill, and the memory wall

**Feature: know what a run cost, and survive one that does not fit.**
Demand-driven — the first model that exhausts memory decides when.

- `bench/` fills for the first time, on real models.
- Invoice counters over the meters that already exist.
- **Collection actually wired**, resolving H1 and H5 rather than
  documenting them: roots as `strong` handles, cross-table marking, the
  operation cache cleared or holding `certificate`s.

*Gate: a model that exhausts memory under the current unbounded tables
completes under a stated policy, with the invoice showing what was paid.*

## Open — R3: the parity gate

**Feature: speed. The existential test.**

- The bounded-integer / FDD leaf theory, porting the legacy flat engine's
  storage (trailing-array nodes, the single-TU hot loop).
- Benchmarks against libDDD and ITS-tools as external baselines on P/T
  saturation over the standard corpus.
- The two convergence accelerators deliberately absent so far, chosen
  **by measurement**: the fused re-saturation in a crossing reply
  (libDDD's `recFireSat`) and libDDD's DFS/BFS knob. Both are convergence
  heuristics; neither changes the answer.

*Gate, stated plainly: parity with legacy on the declared regime, or the
interface work goes back to the substrate with the measurements in hand.*

## Open — R5: breadth, and the paper

- A GAL importer, so existing models run.
- An ω-word / recognizable-language leaf theory, from the aut2ltl work.
- The paper's experimental section generated from the invoice.

*Entry requires R3's gate passed.*

---

## Out of scope: choosing the shape

**Our input is shaped.** NUPN carries a unit tree; the surface's structs
and arrays declare one; a GAL type hierarchy is one. libHSC consumes a
declared shape and refuses to guess.

Deriving a good shape from an unshaped model is a real problem with a known
shape of answer — community detection over a dependency graph, with control
edges distinguished from action edges — and it is a *specification
rewriting* problem that belongs upstream of the library, in the tooling
that produces our input (the Louvain pass in `tools/` and the FORCE order
in `hsc/order/` are exactly that tooling). It is all compromises: Hanoi is
best in blocks of four or five rings, worse as a pure spine, worse again
fully balanced.
