# From libDDD/libITS to libHSC — goals, correspondence, plan

*The bridge document. The calculus drafts (`hsc_core*.md`) define the
target independently of any code base; this document faces the other way:
it states why the reboot exists, maps each concept of the calculus to the
legacy artifact that already implements it (or to the hole where nothing
does), records dispositions for legacy features that do not carry over,
and stages the work. It is the entry point for engineering sessions.*

*Status: written at project creation, against draft 3 of the calculus
(`hsc_core3.md`) and the Python prototype (`python/hsc/`, which tracks
draft 1). Revise it when either moves.*

---

## 1. Why a reboot, in one paragraph

Twenty years of libDDD/libITS invented the right concepts under
engineering duress — skip, selectors, ranges, saturation by locality
partition, equiv-split, local application — but invented them scattered,
half-generalized, and stated nowhere. The historic construction order
(flat DDD first, hierarchical SDD bolted on top, the GAL expression
engine bolted on the side) is the reverse of the conceptual order, and
the cost of that inversion is permanent: every rule exists in up to four
variants (Hom, SHom, Monotonic, MLHom), each generalization is a project
that "never got time" (equiv-split was never ported to SDD), and the
rewriting system that is the library's real intelligence is discoverable
only by archaeology. libHSC inverts the construction: **the hierarchical
calculus is primitive; the flat, fast cases are recognized degenerations
that must compile back down to today's performance.**

## 2. Goals

G1. **One core.** One diagram type (internalisation as the type rule, not
    a feature), one operations basis, one saturation schedule, one
    rewriting system — written once, applying at every sort. The
    DDD/SDD duplication becomes structurally impossible, not merely
    repaid.

G2. **`split_equiv` as the base.** The query/case bracket is the citizen;
    filters, assigns, `ite`, crossing events are obtained from it. Plain
    homomorphisms are the degenerate case in which no query is ever
    posed. Corollary 3.6 of draft 3 (diagrams are a theory, hence export
    `split_equiv`) *is* the generalization of equiv-split to hierarchy —
    a corollary in the new frame, an unfunded project in the old one.

G3. **Theories as contractual imports.** The tiered support algebra
    (E/J/G/B) + `split_equiv` + whole local terms + exported maps
    replaces the informal `DataSet` interface. The enumerated theory is
    the reference implementation and the permanent differential-test
    oracle. LIA + arrays is the first interchange theory (it generalizes
    the existing GAL Expr engine, so it is the testbed with a known
    baseline). Word / ω-word recognizer theories (from aut2ltl) come
    after the seam is proven.

G4. **Performance is a gate, not a hope.** The declared regime over the
    enumerated/FDD theory must monomorphize (templates/CRTP, not virtual
    dispatch) down to an inner loop equivalent to today's flat engine.
    Concretely: parity with libDDD on P/T-net saturation benchmarks
    (philosophers, the usual Petri corpus) before any general-path
    feature is accepted. Every degradation the math recognizes must be
    recoverable by the compiler.

G5. **Memo/GC as first-class policy.** No cache is sacrosanct —
    saturation memos included: "a node is saturated once, ever" is the
    bound achieved under full retention, not a requirement, and every
    memo is an ordinary staleness bet. What the reboot must add is the
    *possibility* of selective retention, which legacy structurally
    lacks: id recycling forces wipe-all-at-GC because an entry citing a
    dead id is indistinguishable from one citing that id's next life.
    Hence: attributable invalidation (generation-tagged codes), cache
    retention policies as pluggable, declared objects, and the
    cost-model invoice counters and the retention policies reading the
    same meters. Cache keys are made context-free so entries are
    reusable in principle (the composite layer's context-currying,
    generalized); whether they are *retained* is policy. This is where
    the long-standing cache/GC feature goals land.

G6. **Modern base.** C++20/23, CMake, concepts for the theory contracts,
    transparent lookup in the intern tables (kills the tmpid probe-slot
    hack), no vendored sparsehash, no REENTRANT/TBB ifdef forest, clean
    repo (no tracked build artifacts).

G7. **The surface.** SMT-flavored declarations (sorts, theories,
    interchange declared; alphabets discovered), an interactive mode in
    the SMT spirit. The surface never mentions cuts.

G8. **The paper.** The hierarchical concepts get their airing as a
    calculus — not as application papers. The library becomes the model
    of the paper rather than its apology; `hsc_core` current draft is the
    seed.

## 3. Concept ↔ code correspondence

Where the calculus already runs in legacy code. References are to
`~/git/libDDD` and `~/git/libITS`; section marks to `hsc_core3.md`.

| HSC concept | Legacy artifact | Status |
|---|---|---|
| Canonical decomposition, (D)(F) (Thm 3.1) | SDD canonical form: arcs partition, sons distinct (`ddd/SDD.cpp`) | Implemented; the theorem is its spec |
| Construction 3.3 (incremental insert: meet, carve by relative difference, regroup by sub) | The SDED union algorithm (`ddd/SDED.cpp`) | Implemented, essentially verbatim; port as *the* canonicalizer |
| Support algebra tiers E/J/G (§2.1) | `DataSet` interface: `set_intersect/union/minus`, `empty`, `set_equal`, `hash` (`ddd/DataSet.h`) — note: no complement, i.e. tier G, no top | The interface is tier G+E avant la lettre; needs tiering vocabulary, `split_equiv`, and local-term delegation added |
| Internalisation (Thm 3.5) | `DDD : public DataSet` (`ddd/DDD.h`), SDD-valued SDD arcs, `IntDataSet` | Exists as a feature; must become the type rule (one diagram type) |
| Local terms handed whole to a theory (Def 2.3) | `localApply` / `SLocalApply`; DFS strategy pushing `fixpoint(h * id)` inside arcs (`ddd/SHom.cpp`) | Delegation structure present; the "theory fuses the term" contract is implicit |
| Skip as id-propagation, compositional (§5) | `skip_variable` + per-class all-parts-skip in Add/And/Compose/Fixpoint, duplicated across Hom.cpp and SHom.cpp | Implemented ~6 times; write once |
| Skip oracles: static support / resolved indirection (§5.2) | `get_range()` (static), the Expr engine's currying (dynamic reach) | Both oracles exist; not unified under one question |
| Saturation schedule F/L/G, chaining (§6.2) | `Fixpoint::eval` in `ddd/SHom.cpp` (~line 1990): `F_part(d2)`, `L_part(d2)`, chained G loop; BFS/DFS strategy knob | Line-for-line the schedule; exists twice (Hom/SHom), plus a third fixpoint family (`Monotonic`) |
| Crossing event as case with fused re-saturation (§6.3) | `recFireSat` and the "nice form l&f" test (`ddd/SHom.cpp` ~2010–2090) | The Kronecker regime detection; becomes a cost regime, not a case distinction |
| Queries, residuals, currying (§7) | `queryEval` / `InfoNode = AdditiveMap<IntExpression,GDDD>` / `QueryCache` (`its/gal/ExprHom.cpp` 112–216) | The LIA instance of `split_equiv`, complete; flat-DDD only |
| Term-grounding order, indirection (§7.4) | `getFirstSubExpr`, `getSubExprExcept`, the `tab[tab[x]]` paths in `_Assign`/`_InvertExpr` | Implemented; the measure was never stated |
| Case (§8), crossing expansions (§8.4) | The `eval` bodies of `_Assign`, `_Predicate`, `_SyncAssign` (`its/gal/ExprHom.cpp`) | Hand-rolled instances; the construct and its contract (§8.6) are not citizens |
| Guard algebra without top (Thm 4.4) | `negate()` = `d − h(d)`, `NotCond`, `is_selector` = the interval `[0,id]` (`ddd/Hom.cpp`) | Shipped; the theorem is its retroactive justification |
| Commutation-enabled rewrites | `notInRange` / `commutative`, `And` normal form via `addCompositionParameter`, guard-hoisting in the `fixpoint()` factories (`ddd/Hom.cpp` 1873–2257, `ddd/SHom.cpp` 2828–2943) | Implemented as dynamic_cast pattern-match chains; different final equations in the two layers; becomes the §4.8 rewriting system, written once |
| Declared vs discovered regimes (§12) | `Hom_Basic` / P/T-net homs (never query) vs the GAL Expr engine (always query) | Both regimes run today, unnamed and unpriced |
| Intension vs extension (§11) | The 2002 decision for homomorphisms over transition-relation DDs; the DED cache as accumulated trace | Thm 11.1 is that decision, proved |
| Currification (§2.6) | The `PIntExpression`/`env` split; per-type `GalOrder` (dense, 0-based) shared by all instances of a type via the model registry (`findType`/`TypeCacher`) | Exists at declared-type granularity — instances share expression/hom codes and caches; identity is nominal (name registry + `GalOrder*`), not positional — see D1 |
| Codes / interning | `UniqueTableId` (id-based, nodes), `UniqueTable` (pointer-based: homs, expressions), separate stacks for DDD/SDD/Hom/SHom/MLHom/expressions | One mechanism, ~6 instantiations; unify, add transparent lookup, keep the id design |
| GC / memo lifecycle | `MemoryManager`, `GCHook`, wipe-all caches at GC; `QueryCache`'s `CacheHook` | The negative example: G5 exists because "saturated once, ever" is false under wipe-all |

## 4. Gaps — no prototype anywhere

D1. **Position-relative codes beyond declared boundaries** (§2.6 full
    strength). The legacy stack already achieves currification *at the
    granularity the modeller declares*: `GalOrder` is per GAL type,
    dense from 0; the model registry identifies types, so all instances
    of a type share one `GalOrder` and therefore one set of expression
    codes, hom codes and cache entries, with the SDD level addressing
    the instance (`localApply`) — this is why scalar/circular sets
    scale. What remains nominal rather than positional: variable
    identity is a global name registry
    (`IntExpressionFactory::var_names`), and hom identity anchors on
    the `GalOrder*` (compared by pointer). Sharing therefore stops at
    declared type boundaries — two isomorphic but separately declared
    types, or isomorphic sub-regions of one flattened GAL with
    qualified names (`p0.x` vs `p1.x`), share only `PIntExpression`
    skeletons, never codes. §2.6 requires isomorphism to be
    *discovered* (positions are paths; equal subterms are one object),
    not declared. The gap, restated: promote the existing
    type-granularity sharing to arbitrary positions. Two hygiene notes
    found while verifying: the `var_names` registry only grows and is
    never collected, and a rebuilt `GalOrder` would silently orphan
    every cache entry keyed on the old pointer (stale, not unsound).

D2. **Kernel/labelling split and the retroactive merge** (§7.5–7.6). The
    QueryCache merges only syntactically identical residuals; partition
    federation does not exist. (The Python prototype has a first cut of
    the kernel merge — see `python/hsc/`.)

D3. **`split_equiv` as the uniform theory export.** `DataSet` exposes the
    partition *window* (meets, differences) but not the bulk
    residual-indexed primitive, so today the engine does to theories
    exactly what Def 2.3 forbids: splits their codes from outside and
    re-joins.

D4. **The case as citizen** with its assume/guarantee contract, plus the
    attributability instruments (sampled linearity / class-determinacy
    checks in debug profiles).

D5. **Disposition selection** (§8.5): the four join-like strategies are
    hard-coded per operation in legacy; selection under the cardinality
    estimates the queries already produce is unowned.

D6. **Durable certificates** (G5): no legacy cache survives collection;
    id recycling in `UniqueTableId` is why. Generation-tagged ids or a
    mark pass over caches are the candidate designs.

## 5. Dispositions for legacy features

* **`top` terminal (approximation/overflow).** Dropped: criteria are
  total, partiality is a typing question settled at declaration (§7.2).
  Needs an explicit error story where legacy used `top` as an overflow
  signal.
* **`invert` / `_InvertExpr` (relative preimage).** Out of scope in the
  calculus, with the argument recorded (Def 2.2: memory-destructive
  assignment; refuse the question rather than price it). The legacy
  implementation already conceded the point by demanding `pot` (the
  reachable set) as an operand. Replacement plan: forward-only checking
  via the has_image/SCC machinery where it suffices; where genuine
  backward steps are unavoidable, a *separate derived layer* over the
  calculus taking the reachable set as a first-class operand. Do not
  port the potall-threading.
* **MLHom / MLSHom.** Superseded: their one real client (the query
  protocol) was rewritten as the side-cache worklist years ago and the
  live equiv-split no longer uses them. Do not port. If an up-down
  protocol is ever needed again, it re-derives from the case construct.
* **`Monotonic`.** A third fixpoint family in legacy; in the calculus it
  is a schedule property (confluent operand base ⇒ any chaining order),
  not a construct. Fold into the saturation schedule's options.
* **EVDDD ifdefs / edge values.** Dropped; semiring codomains are
  classifier business, out of scope by design.
* **The flat DDD engine (`_GDDD`: packed short valuations, trailing-array
  nodes, single-TU hot loop).** *Survives intact* — demoted from core
  type to the code representation of the enumerated/FDD leaf theory.
  This is how G4 is achievable: nothing about its inner loop changes.
* **`UniqueTableId`** (id handles, free list, sparse root-only refcounts,
  mark & sweep). Keep the design — it independently matches what interner
  patterns converged on — with the known fixes: transparent lookup
  replaces the tmpid probe slot, comparators stop going through a
  guarded singleton, sweep swaps instead of copy-assigns, marking gets an
  explicit worklist, ids gain generations (D6).

## 6. Architecture sketch

Layer order, each knowing only the ones before it (the Python prototype
already follows this order — core / classify / ops / leaves):

1. **Codes**: one interning mechanism (UniqueTableId generalized),
   serving nodes, operations, residuals, kernels. Generation-tagged ids.
2. **Theory interface**: tiers + `split_equiv` + `apply_local` (whole
   terms) + exported maps, as C++ concepts. Implementations: enumerated
   (oracle), FDD backed by the legacy flat engine (fast path), LIA +
   arrays (interchange), diagrams themselves (internalisation).
3. **Diagram theory**: one node type; Construction 3.3 as the one
   canonicalizer (ported from SDED); zero as absence.
4. **Operations**: the basis (`id`, `local`, `query;case`, `∘ + ∗`);
   the rewriting system written once; the saturation schedule written
   once; contracts instrumented.
5. **Memo & policy**: caches with retention policies, the invoice
   counters, GC with durable certificates.
6. **Surface**: SMT-flavored declarations, GAL as first client.

## 7. Milestones

M0. *(this commit)* Repo, drafts, bridge document, Python prototype moved
    in.

M1. **Theory seam on paper then in Python.** Re-align the prototype to
    the current draft: `split_equiv` contract per §7 (kernel/labelling,
    merge), the case construct per §8, skip from `id` with the oracle
    ladder. The prototype's scope table (its README) is the worklist.
    Products: revised `hsc_spec` for the seam, prototype passing
    brute-force differential tests.

M2. **C++ theory interface.** The concepts header, written against two
    theories: enumerated (oracle) and `_GDDD`-backed FDD. Differential
    tests between them. No diagrams yet.

M3. **Diagram type + canonicalizer.** Port SDED's union as Construction
    3.3 over the theory interface; internalisation typing; congruence
    tower inspection.

M4. **Operations + saturation, once.** The basis, the schedule, the
    rewrite rules from the legacy factories re-expressed as the §4.8
    system. **Gate: parity with libDDD on P/T saturation** (declared
    regime, no case ever called). This is where G4 is proven or the
    design goes back to M2.

M5. **Query/case + LIA.** Port the ExprHom semantics to the general arc
    type (this is "equiv-split over SDD", now a port not a research
    project). GAL models as the test corpus; the legacy GAL engine as
    the baseline.

M6. **Policy & surface.** Durable certificates, retention policies,
    invoice; the SMT-flavored surface and interactive mode.

M7. **The paper.** From the then-current draft, with the library as its
    model and the invoice as its experimental section.

Each milestone follows the house convention: spec before code, report
after, and the calculus draft revised when implementation pressure
teaches something (that is what "seed of its own iteration" means).
