# Prospective directions — openings from the reductions/contracts thread

Status: screened openings, not commitments. Companion to
`structural_reductions.md` (the program note: proved kernel, staged
engineering); this document holds what lies *beyond* that program —
directions endorsed for development, ideas kept honest about their
speculation level. Sources: the hsc calculus and engines, the SoS
invariant / learner / TGBA-entry papers
(`~/git/BuchiToLTL/research_notes/sos_core.md`, `sos_learning.md`,
`sos_fromTGBA.md`, `sos_calculus.md`).

## 1. SoS as a pluggable leaf theory  *(endorsed — design intent)*

hsc's leaf contract is three maps — join, meet, relative difference —
with equality and emptiness discharged by interning. The SoS calculus
supplies exactly these on same-table invariants: Boolean surgery on
saturated pair sets over a shared stamp is `O(|P|)`, complement one
flip, and byte-canonical serialization is the interning. So an
**invariant-valued leaf theory** is admissible in the calculus as it
stands: a position in a shape can hold a behavior. The stamps were
designed for this fit — they classify finite words/prefixes while
carrying long-term acceptance, which is exactly what a leaf must
answer locally while the diagram above it composes.

The one design point is the **exchange alphabet**: the leaf's letters
are the currency between the diagram algebra and the language algebra.
Cross-table operations pay `align` (the generated product — only
realizable class pairs materialize, the calculus's own on-the-fly);
same-table operations are free. A leaf theory should therefore hold a
*family* of invariants over one shared table per sort where possible,
re-aligning only on theory growth.

First verification step (small): implement the leaf with
meet/join/minus as align-then-surgery, intern by serialized bytes, and
exercise it as the recognizer leaf of the contract program — the
substitution scalar of Theorem 2 becomes a native leaf instead of an
encoded one.

## 2. Shape-directed CEGAR  *(endorsed — develop; the strongest opening)*

Abstraction units are components; the refinement order is the shape
tree; abstractions are canonical.

**The lattice.** Each discovered component independently sits at a
rung: chaotic (Σ^∞) ⊒ over-approximating learned hypotheses ⊒ exact
contract (recognizer leaf) ⊒ concrete subtree. A global abstraction is
a **profile** assigning each component a rung. Every rung above
"exact" over-approximates (sound for invariants); "exact" is
definitive for V-properties (Theorem 2); "concrete" is ground truth.

**The loop.** Check the property under the current profile — cheap,
since most components are small recognizer scalars.

* *Invariant proved* → done, at any rung.
* *Witness found* → **localized replay**: by Theorem 2's run
  decomposition, the witness is a context sequence α plus one sync
  word w_i per component. Check each `w_i ∈ Pref(componentᵢ)` — a
  membership query per component. All pass → the witness is real
  (Theorem 2 realizes it). Some component rejects its w_i → **that
  component is the refinement target**. Spurious witnesses *name their
  culprit by construction*: no abstract-counterexample analysis, no
  heuristics — membership queries decide.

**CEGAR = MAT.** The refinement step for the culprit is exactly the
learner's counterexample step: feed w_i to the component's MAT
instance — the hypothesis splits at least one class, irreversibly. The
model checker's spurious witnesses *implement the equivalence oracle*;
the two loops are one loop. Termination: each refinement splits a
class or descends the finite rung ladder, so the total is bounded by
Σᵢ (#classes of component i's exact contract) — the same
O(#classes) that prices the learner, now pricing verification.
Refinement policy is free to vary: descend a rung wholesale, or refine
the hypothesis in place; culprit choice when several reject (rarest
first, cheapest first) is a measurable knob.

**Cost shape.** Components whose behavior the property never probes
stay chaotic forever — the cone of influence *emerges* from the loop
instead of being computed up front. Components probed only shallowly
stabilize at coarse hypotheses. Only the load-bearing components ever
approach their exact contract or concrete form. Interning multiplies
the effect: refining one component's contract refines every symmetric
instance simultaneously (§3).

**Placement.** This was visible from GAL/ITS days — hierarchical
decision diagrams share sub-structures, which is implicit abstraction —
but never pursued as an explicit CEGAR discipline; the missing pieces
were canonical per-component abstractions and the localization
theorem, both now in hand. Entry point after the collapse spec ships:
the loop needs only Theorems 1–2, the learner, and the projection
differential as the final gate.

## 3. Symmetry from contracts; bag shapes; toward parametric

The current symmetry threads reproduce the ITS machinery — encoding
and saturation stress-tests, structural symmetries. This direction is
complementary: **detect symmetry semantically, then make the shapes
match**, rather than hoping the shapes already exhibit it.

**A priori detection, three stages.**

1. *Signature pre-filter* (cheap, syntactic-modulo-renaming): group
   subtrees by (multiset of leaf domains, event counts per support
   pattern, interface arity) — invariant under internal reordering, so
   shape mismatch does not hide candidates.
2. *Semantic confirmation*: within a group, compute or learn each
   member's interface contract and test **equality up to alphabet
   renaming** — which is literally an operation of the SoS calculus
   (relabel = inverse substitution, one reduce, one byte comparison,
   with canonicity pruning: class counts and letter-class profiles
   must match before any bijection is tried). The label bijection is
   usually forced by the mediator wiring (align letters by their
   shared-variable footprint). Contracts are shape-independent, so
   this finds symmetries structural methods cannot: differently
   written, differently ordered, differently encoded components that
   are behaviorally identical at their interfaces.
3. *Exploitation — reshape by substitution, then quotient.* Replacing
   every instance by the **same interned recognizer leaf** makes the
   reshaping problem trivial: all instances become one code, and
   diagram sharing over them is perfect by construction (the "the
   hierarchy is not bad at encoding it, sometimes" effect, made
   unconditional). Then, under a parent whose events are invariant
   under permutation of k identical children — now checkable cheaply,
   the children being equal codes — replace the tuple node by a
   **bag node**: a shape constructor `bag(theory, k)` whose states are
   multisets, i.e. counts per contract state. Parent events act on
   counts. Exact under full symmetry (star/clique via a shared
   mediator); the support algebra of a bag (meet/join/minus on
   count vectors) is a well-defined, bounded engineering object.

**Parametric.** Make k symbolic: the bag node over parameter n, with
count dynamics that are vector-addition-shaped. Learn the contract
once, verify for all n — the bridge from today's machinery to
parameterized verification, and the natural meeting point with the
parametric thread. Honest boundary: partial symmetry (rings) needs
cyclic quotients, not bags — later, harder.

## 4. Residuation: the weakest assumption as surgery  *(grounded in the calculus)*

The calculus already contains the pieces. **Rootings** are left word
quotients (`u⁻¹L` as pair surgery, composing as a right M-action) —
the assumption *after a history*, incremental AG along a trace, free.
For the composition-level quotient: with interface-aligned alphabets,
`C ∥ X ⊨ φ  ⟺  L(X) ⊆ ¬(L(C) ∩ ¬φ)`, so the weakest assumption is

    W = ¬L(C) ∪ φ

— on an aligned table, **one flip and one union**: free surgery. The
genuinely priced move is the alphabet: projecting the context's
behavior onto the interface letters is the calculus's exponential
frontier when the erased propositions are constrained — and the
free-proposition read-off prices it *before* it is paid. So the
strategy writes itself: **compute W by surgery when the projection is
affordable or the alphabet is already the interface; learn W when it
is not** — two acquisition routes, one lattice, byte-identical
targets, and the choice is made by a read-off, not a guess. AG rules
become equations in a finite (residuated) Boolean algebra of
saturated pair sets; assumption reuse is interning, as everywhere.

## 5. The contract census; behavioral bricks  *(two tiers, second explicitly ambitious)*

*Tier 1 — measurable.* Run collapse + canonicalization over the corpus
and count distinct contracts. Prediction: heavy-tailed — thousands of
components, few dozen behaviors (mutex, buffer, barrier, counter mod
k). If confirmed: per-component analysis (closure, recognizer,
invariant, learned assumption) is memoized per *contract*, a
superlinear win; and the census itself — the distribution of component
behaviors in the wild — is a publishable empirical object that only
canonical identity makes possible.

*Tier 2 — speculative, screened as such.* Compositions are never
trivial: the product walk remains the irreducible cost, and the census
amortizes leaves, not assemblies. The ambitious reading — recognizing
whole *assembly patterns* (a ring of k instances of contract 𝓘, a
star around mediator M) and attaching pattern-level verification
lemmas to them, so that recognized assemblies inherit known results —
is real research, adjacent to parameterized verification (§3), and
should be treated as a destination, not a step.

## 6. An MDL objective for process discovery  *(tentative)*

Discovery currently optimizes modularity heuristics. Candidate
principled objective: minimize the summed interface-contract
complexity (class counts of the relativized invariants) over the
partition. What it brings: decomposition quality measurable *before*
any verification runs; a stopping rule (stop splitting when total
contract complexity rises); and a direct predictor of learning cost,
since the learner converges in O(#classes). Status: plausible,
unvalidated — one corpus experiment (compare against Louvain cuts on
models with known process structure) would decide whether it earns a
place.

## 7. Quantitative contracts via finite idempotent semirings  *(kept, cut down)*

The TGBA entry's profile construction is parametric in the semiring:
it needs a finite carrier, idempotent monotone addition, and finitely
many Ramsey colors. That excludes genuine tropical semirings (infinite
carrier) and probabilities (non-idempotent) — but **saturating
counters** ((min,+) or (max,+) capped at k, then saturate) qualify on
all three counts. The corresponding contracts answer bounded-response
questions at interfaces: "the component grants within ≤ k syncs",
promptness rather than bare eventuality. Scope: one exploratory
sitting to check the Ramsey argument survives the capped carrier
verbatim; if it does, bounded-liveness contracts come from a semiring
swap, no new theory.

## 8. Peripheral: black-box teachers  *(application note, out of scope here)*

Nothing in the MAT loop requires the component to be a spec: any
implementation answering membership queries can be learned, its
canonical contract model-checked in composition, and version drift
detected as a byte diff of invariants. An application of the
machinery, recorded for completeness; not this program's concern.

---

*Method coda.* The discipline that made the parent session fast:
every layer acquired a two-sided oracle before an implementation —
count differentials, projection differentials, byte-parity gates,
prove/replay, cross-entry regressions. Whatever survives from this
document should inherit that rule: no direction graduates without its
oracle named first.
