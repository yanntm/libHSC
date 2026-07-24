# `core/` — the calculus

The calculus core. `algorithm.md` beside this file states every definition
these headers implement. Depends on `util/` and `mem/`; knows nothing about
any particular leaf theory.

* `code.hh` — a **code** is a 32-bit id; code `0` is **absence** (the
  pointed discipline's adjoined empty element). Because absence is not a
  citizen, emptiness is `c == 0` and equality is `a == b`: neither is ever a
  question for a theory.

* `shape.hh` — sorts, `V ::= 1 | ⟨A⟩ | (V_h , V_t)`. Interned trees, so
  no names (a position is a path) and no associativity (`(V₁,(V₂,V₃))` and
  `((V₁,V₂),V₃)` are different codes — refusing that quotient is what
  "hierarchical" means).

* `support.hh` — what a sort exports for its codes to be primes at a cut:
  **join, meet, relative difference**, and nothing else. No
  top, no complement; no construction in libHSC forms one. Type-erased,
  because which algebra a head's primes live in is what the *shape* says and
  shapes are data.

* `diagram.hh` + `src/diagram.cc` — the normal form, the one canonicalizer,
  and the set algebra over it. `diagram_engine` implements `support_algebra`
  like any leaf theory, which is internalisation made structural rather than
  remembered: there is no leaf case and node case, and `⟨Diag(V)⟩` is a
  legal import.

* `operation.hh` + `src/operation.cc` — operation terms, the
  fragment that needs no query: `id | node(H_h,H_t) | ∘ | +`. The term
  **mirrors the shape tree** rather than naming an absolute variable, which
  buys two things libDDD structurally cannot have: skip is literally
  `term == id` (no oracle, no support set), and descending into a subtree
  re-roots the term so isomorphic positions share codes.
  `product()` assembles an event from one maximal local
  term per leaf — which is exactly what a Petri transition or a Hanoi move
  is.

  The leaf case is deliberately *not* in this table: at a leaf sort the term
  is a theory term, read by the theory that owns the sort. A term is
  interpreted by whichever algebra it is handed to, exactly like a value.

  `saturate()` is the static saturation rewrite: partition the events by where
  they reach relative to a cut, close F below and L on the edge recursively
  (hierarchy is automatic), leave G to be chained. **The split is
  syntactic here**: a summand `node(h,t)` is F if `h == id`, L if
  `t == id`, G otherwise. libDDD needs a per-variable partition cache over
  `skip_variable` for this; libsdd a `dynamic_cast` chain. The evaluation
  schedule is libsdd's `_saturation_fixpoint::operator()` verbatim — F, then
  L, then chained G, to stability.

* `manager.hh` — owner of the shape table, the operation terms, the imported
  theories and the diagrams. No singletons: everything reaches its users as
  an argument.

See `algorithm.md` for the algorithms, which are written before the code.

The crossing fragment lives *above* this core: `hsc/event.hh` and
`hsc/query.hh` own the case bracket (crossing guards, updates and
comparisons via `split_equiv`), and register their evaluator with the
manager; core stores and hashes `expr` terms but never reads them.

## Deliberately absent

Two accelerators the reference engines have and we do not: the fused
re-saturation in a crossing reply (libDDD's `recFireSat` and the `l&f`
nice-form test), and libDDD's DFS/BFS knob (an inner fixpoint per `g`
instead of one application). Both are convergence heuristics that profile
differently per workload; neither changes the answer. They arrive when a
measured workload chooses between them.
