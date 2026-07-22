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

* `manager.hh` — owner of the shape table, the imported theories and the
  diagrams. No singletons: everything reaches its users as an argument.

See `algorithm.md` for the algorithms, which are written before the code.

## Pending

Operations (`id`, `local`, `query;case`, `∘`, `+`, `∗`) — M4. `split_equiv`
as the diagram theory's export (Cor. 3.6's *other* half) — M4.
