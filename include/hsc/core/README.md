# `core/` — the calculus

Reading these headers against `research_notes/hsc_core4.md` should be a
section-by-section correspondence. Depends on `util/` and `mem/`; knows
nothing about any particular leaf theory.

* `code.hh` — a **code** is a 32-bit id; code `0` is **absence** (discipline
  1's adjoined `0`). Because absence is not a citizen, emptiness is `c == 0`
  and equality is `a == b`: neither is ever a question for a theory.

* `shape.hh` — sorts, `V ::= 1 | ⟨A⟩ | (V_h , V_t)` (§1). Interned trees, so
  no names (a position is a path) and no associativity (`(V₁,(V₂,V₃))` and
  `((V₁,V₂),V₃)` are different codes — refusing that quotient is what
  "hierarchical" means).

* `support.hh` — what a sort exports for its codes to be primes at a cut
  (§2.1, tier G): **join, meet, relative difference**, and nothing else. No
  top, no complement; no construction in libHSC forms one. Type-erased,
  because which algebra a head's primes live in is what the *shape* says and
  shapes are data.

* `diagram.hh` + `src/diagram.cc` — the normal form (Thm 3.1), Construction
  3.3 as the one canonicalizer, and the set algebra over it. `diagram_engine`
  implements `support_algebra` like any leaf theory, which is Corollary 3.6
  made structural rather than remembered: there is no leaf case and node
  case, and `⟨Diag(V)⟩` is a legal import.

* `operation.hh` + `src/operation.cc` — operation terms (§4.1), the
  fragment that needs no query: `id | node(H_h,H_t) | ∘ | +`. The term
  **mirrors the shape tree** rather than naming an absolute variable, which
  buys two things libDDD structurally cannot have: skip is literally
  `term == id` (discipline 3, no oracle, no support set), and descending
  into a subtree re-roots the term so isomorphic positions share codes
  (§2.6, gap D1). `product()` assembles an event from one maximal local
  term per leaf — which is exactly what a Petri transition or a Hanoi move
  is.

  The leaf case is deliberately *not* in this table: at a leaf sort the term
  is a theory term, read by the theory that owns the sort. A term is
  interpreted by whichever algebra it is handed to, exactly like a value.

  `saturate()` is the static rewrite of §6: partition the events by where
  they reach relative to a cut, close F below and L on the edge recursively
  (§6.4 — hierarchy is automatic), leave G to be chained. **The split is
  syntactic here**: a summand `node(h,t)` is F if `h == id`, L if
  `t == id`, G otherwise. libDDD needs a per-variable partition cache over
  `skip_variable` for this; libsdd a `dynamic_cast` chain. The evaluation
  schedule is libsdd's `_saturation_fixpoint::operator()` verbatim — F, then
  L, then chained G, to stability.

* `manager.hh` — owner of the shape table, the operation terms, the imported
  theories and the diagrams. No singletons: everything reaches its users as
  an argument.

See `algorithm.md` for the algorithms, which are written before the code.

## Pending

`query;case` and `split_equiv` as the diagram theory's export (Cor. 3.6's
other half) — needed for cross-level *arithmetic* (`x := y + z`, `tab[i]`)
and nothing before it.

Two accelerators the references have and we do not, both deliberate: §6.3's
fused re-saturation in a crossing reply (libDDD's `recFireSat` and the
`l&f` nice-form test — found empirically in `demo/hanoi/hanoiHom.cpp` v6,
"the test resaturates before returning"), and libDDD's DFS/BFS knob (an
inner fixpoint per `g` instead of one application). Both are convergence
heuristics that profile differently per workload; neither changes the
answer. They arrive when there is a workload to choose between them.
