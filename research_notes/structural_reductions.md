# Structural reductions in hsc — a working note

Status: idea note, pre-spec, second iteration. Grew out of the
rewrite-chain work (chain, passes, two-engine differential in place; see
`include/hsc/surface/algorithm.md` §1c and `handoff_explicit.md`). Goal:
agglomeration (Berthelot pre/post, Lipton transaction reduction) and
partial-order reduction, re-expressed over guarded commands with the
analyses we already compute — supports (ctrl / reads / writes per event,
sorted sparse sets, `readers(p)` inverse) and inferred value domains.

Grounding (papers/, untracked): `thierrymieg2021_symbolic_structural_mc`
(the 22-rule system; agglomerations are rules 14–16),
`berthomieu2018_reductions_counting_markings` (reductions as linear
equations, counting), `amat2022_polyhedral_abstraction_smt` (formalizes
Berthomieu's triples as E-abstraction equivalence),
`laarman2018_stubborn_transaction_reduction` (TR with stubborn-set
commutativity, beyond Petri nets).

## The contract

Fix a set **V of variables of interest**. A reduction is correct when
the **projection of the reachable set onto V is unchanged**; runs may
gain or lose hidden-only stutter steps, so the guarantee extends to
stutter-insensitive LTL over V. Counts are *not* preserved by default,
so the `reach == xreach` differential does not test these passes. The
oracle is the **projection differential**: hide the non-observed
coordinates and compare projected reachable sets, full vs reduced — in
the explicit engine a strip-and-dedup, symbolically an existential
quantification (to build).

**Optional upgrade (Berthomieu).** Counts become reconstructible if each
reduction records its inverse map, à la the (N₁, Q, N₂) triples where Q
is a linear system over markings and R(N₁) is recovered from R(N₂) ∧ Q.
Our (x, m) handshake has a non-linear but simple inverse: every deleted
intermediate state is s[x↦m] for a reduced state s enabling a producer,
so |R_orig| = |R_red| + |{s ∈ R_red : some producer enabled at s}|
(modulo states reachable both ways — s[x↦m] already in R_red is not a
new state; with exclusive mediation and consumers-exit it never is,
since x==m arises only as an intermediate). A recorded-inverse pass
would restore an *exact-count* differential — a far stronger oracle than
projection comparison. Not v1, but the design should not preclude it.

**Amat's refinement (polyhedral abstraction) — three ideas to steal.**
(1) *E-transform the property, not just the state space*: with
(N₁,m₁) ⊳_E (N₂,m₂), checking invariant F on N₁ reduces to checking
∃x⃗. Ẽ(x⃗,y⃗) ∧ F(x⃗) on N₂ — the query is rewritten through the
reduction. Consequence: the support need not be fixed a priori; even an
*observed* variable may be reduced as long as its relation is captured
in E (their explicit contrast with cone-of-influence slicing). For us:
V = free variables of the query is the v1 contract, but the mature form
carries a constraint system alongside the rewritten spec and rewrites
queries through it. (2) *Equivalence as a congruence*: E-abstraction is
preserved by chaining (E ∧ E′, fresh-variable hygiene), synchronous
product, and relabeling/hiding — so each rule is proved once as a tiny
local axiom and applied inside any context. Observation-sequence
equality (silent τ vs labeled transitions) is what buys the congruence;
it is exactly our hidden-effect / stutter distinction, formalized. This
fits the rewrite chain's provenance contract: the per-pass trace's
mature shape *is* the constraint system E, and pass composition is
Theorem "chaining". (3) Their E stays conjunctive-linear because place
agglomeration keeps sums (a = p + q, all token distributions reachable).
Our (x,m) handshake is lossy on states, so our inverse map is not
linear — but it is still QF-LIA as a disjunction with substitutions:
reach_orig(s) ⟺ reach_red(s) ∨ (∃t₁ ∈ P: s = s'[x↦m] ∧ reach_red(s')
∧ g₁(s')). The E-transform machinery carries over unchanged.

## Agglomeration as a handshake on (x, m)

A candidate is a hidden variable x with a mediating value m — "place p"
with `x == m` as "p marked". From the analyses:

* **Producers P** — every event writing `x := m` (the automaton
  discipline pins their source values).
* **Consumers C** — every event whose guard carries the conjunct
  `x == m` and which exits (`x := m' ≠ m`).

Fusion replaces P and C by the products t₁.t₂ (an alt over C per
producer): guard `g₁ ∧ wp(a₁)(g₂ ∖ {x==m})` — wp is `lia` substitution,
usually the identity — body the concatenated do-sequences; the
`x := m` write and the `x == m` test annihilate. States with `x == m`
vanish. Afterward, re-run the domain inference: m may leave x's value
set, sometimes collapsing it to a singleton — then `simplify-constants`
deletes x entirely. Reductions compose into a shrinking loop, applied to
fixpoint with re-analysis between rounds, as in the Petri suites.

**Gain-freedom is structural.** Any firing of the fused event replays in
the original as adjacent t₁;t₂ at the same instant (g₁ holds, `x := m`
establishes `x == m`, wp carries the residual guard). So under any
condition set the contract can only fail by *loss* of projected states;
the differential is one-sided.

## Condition 6 is a mover condition (Lipton)

The fused event fires at one instant; relative to an original run
u t₁ w t₂ v, one of the two endpoints has been *moved* across w, and the
moved event must commute with everything that can interleave. The
x-machinery is free in both directions (the annihilated test, and w
blind to m by exclusive mediation); what remains:

* **Fuse late, at the consumer's instant** — t₁ moves right past w. If
  t₁ is silent (writes only x) its *effects* commute for free; the only
  obstruction is its *guard*: w may disable g₁. Discharge **(6a)**: per
  atom of g₁ over a variable some third event writes, every constant
  write satisfies the atom (enabling writes are harmless — they add
  fused runs that existed as reorderings). Value-sensitive
  enable/disable classification, shared with POR.
* **Fuse early, at the producer's instant** — t₂ moves left past w. Its
  *guard* commutes for free only when it is exactly the mediation test —
  **(6b)**: g₂ ≡ `x == m` — but its *effects* do not: additionally need
  **(6b′)**: for every third event e,
  `(writes(t₂) ∖ {x}) ∩ (reads(e) ∪ writes(e)) = ∅`.
  Write-write conflicts count: even on a hidden y, reordering `y := 1`
  against `y := 2` diverges later. (6b′) is exactly the POR independence
  relation — one analysis serves both passes.

**(6b) alone is unsound** — a counterexample, all of (1)–(5) + trivial
consumer satisfied: x hidden, y z observed, all initially 0, m = 1;
t₁: `z==0 → x:=1`; t₂: `x==1 → x:=0, y:=1`; e: `y==0 → z:=1`.
Original: t₁;e;t₂ reaches (y,z)=(1,1). Fused f: `z==0 → y:=1` and e
disable each other; (1,1) is lost. The failure is t₂'s effect `y:=1`
moved left across e, which reads y. (6b′) refuses: writes(t₂) ∋ y ∈
reads(e).

**Why Petri nets don't pay (6b′) — two net freebies, not one.**

1. *Guard-as-consumption.* A net transition consumes its input tokens at
   its own instant; interleaved transitions provably never touched them,
   so delaying h is always safe — the free agglomeration (rule 16 of the
   22-rule system) needs no persistence at all for safety. A guarded
   command's guard is a pure test, no reservation: we pay (6a) to delay,
   or fuse eagerly.
2. *Additive, monotone effects.* A marking after a sequence depends only
   on the Parikh count, not the order; producing f's output tokens early
   commutes and can only enable more. Assignments neither commute nor
   are monotone — that is the freebie the counterexample exploits, and
   why eager fusion pays (6b′). The clause is vacuous on nets, which is
   why no analogue appears in any of the net rule sets.

Note the transition-centric *pre*-agglomeration (rule 14), which also
preserves deadlocks, does pay persistence even in nets: strong
quasi-persistence, ∀p₂ ∈ •h, p₂• = {h} — no third transition can
consume h's inputs. That is (6a) by other means. So "the net gets
condition 6 free" is true only of the safety-only free variant.

### Pre-agglomeration (producer silent)

1. **Hiding**: x ∉ V.
2. **Silent producer**: writes(t₁) = {x} exactly. The intermediate state
   projects on V like its predecessor, so deleting it loses no V-point
   even when a run dies there — no liveness side condition for projected
   reachability (the analogue of safety-only free agglomeration).
   Relaxation to hidden-only writes: sound iff writes(t₁) ∖ {x} is
   additionally independent of third parties, the (6b′)-shaped clause on
   the producer side — the mover frame answers this former open
   question; no chain argument needed.
3. **Exclusive mediation**: nobody outside C can see m. Strong form: no
   other event reads x. Value-refined: other readers are fine if every
   guard atom on x excludes m (`== k`, k ≠ m; `!= m`); a value-read of x
   is fatal.
4. **Consumers exit**: each t₂ leaves x ≠ m.
5. **No initial marking**: seeds have x ≠ m.
6. **Mover discharge**, either suffices: **(6a)** (fuse late), or
   **(6b)+(6b′)** (fuse early).

### Post-agglomeration (producer visible)

Consumers must be silent on V (writes(t₂) ∩ V = ∅) so the erased
intermediate projects like the fused post-state. Since t₁ is visible it
can be neither skipped nor moved: fusion is forced early, so
**(6b)+(6b′) are both mandatory** — silence on V alone is *not* enough,
a hidden variable written by t₂ and read by a third party breaks
commutation exactly as in the counterexample. With (6b′) the classic
progress caveat discharges statically: a stranded suffix t₁;w maps to
(t₁t₂);w with identical V-projection.

### Deadlock-over-V as an optional preserved property

The net price list (rule 14 vs 16): quasi-persistence — without it a
third event enabled at the pre-t₁ state but not at the post-t₁ state
makes the fused net live where the original deadlocks — plus
divergence-freedom (no t₁^ω runs). For us: (6a) in both directions
(neither disabling *nor* enabling writes on g₁'s support) plus a
t₁-cannot-self-loop check. Forces the late-fusion side. Keep out of v1.

### Mapping to the Petri structural conditions

token-in-place ↦ value-in-variable; `•p = {h}` ↦ P as computed;
"no other transition tests p" ↦ the value-refined reader scan;
`h• = {p}` ↦ writes(t₁) = {x}; `•f = {p}` ↦ (6b); strong
quasi-persistence ↦ (6a); token consumption at h's instant and additive
effects ↦ the two freebies above, replaced by the explicit mover
discharges. Every check is a support intersection or an
atom-vs-constant-write comparison.

## The Lipton dictionary (Laarman's characterization)

Laarman's stubborn TR makes the Lipton frame explicit and dynamic; our
handshake is its static, spec-level instance. The L1–L4 dictionary:

* **L1** (pre-phase right movers) ↦ t₁ moved right: silence covers the
  effects, (6a) covers the guard.
* **L2** (post-phase left movers) ↦ t₂ moved left: (6b) covers the
  guard, (6b′) covers the effects.
* **L3** (transactions don't block) ↦ dropped for projected
  reachability, exactly as TR "prunes irrelevant deadlocks" — the
  free-agglomeration move. Laarman needs only a weakened L3 (every
  post-state reaches an external state, enforced by making bottom-SCC
  roots external); we need none, because the silent producer makes the
  stranded intermediate projection-invisible.
* **L4** (pre must not disable φ, post must not enable φ) ↦ x ∉ V for
  pre; writes(t₂) ∩ V = ∅ for post. TR splits visibility into
  enabling-in-pre / disabling-in-post where POR must take the union —
  one more reason the two reductions don't subsume each other.

Three structural facts from his comparison worth keeping:

* **POR cannot use right-commutativity** (a →-stubborn set is an
  invalid POR: two locks look independent and a deadlock is missed);
  TR can, in the pre-phase. So agglomeration and stubborn-set POR are
  genuinely complementary — TR flattens long sequential blocks (2ᵖ vs
  n·p states on p independent threads of length n), POR wins on massive
  parallelism. Run agglomeration first, POR on the residual.
* **Dynamic movers are per-state stubborn closures**: Mᵢ→(σ,α,σ′)
  demands a semi-stubborn B with B ∩ en(σ) = {α} — "no enabled remote
  action interferes from here on", predicted via NES closure over
  disabled actions. Our (6a)/(6b′) are the whole-spec (all-states)
  approximation of the same checks; a per-state refinement in the
  explicit engine is a possible later upgrade, and his monotonicity
  lemmas (moverhood survives remote steps) are what a correctness proof
  would lean on. Optimal per-state TR is polynomial (deletion algorithm
  with the thread's actions pinned) where minimal stubborn sets are
  NP-complete — the tractable side of the spectrum.
* **STR is process-bound; process-less STR is named an open problem.**
  Our handshake is exactly a process-less transaction: the sequential
  skeleton comes not from a program counter but from the variable's
  automaton discipline — x's value chain *is* the thread. That framing
  positions the hsc contribution against his open problem and deserves
  a place in any paper version.

## Local semantic discharge (Amat-style, minus the traceability)

Every side condition — (3), (6a), (6b′) — is a ∀-statement over
reachable states, so any **over-approximation** of reach discharges it
soundly. We need none of Amat's exact reconstruction: the rule schema
is proved once on paper (the mover analysis above); an instance only
needs its hypotheses established. Escalating discharge ladder:

1. **Syntactic**: support intersections + atom-vs-constant-write (v1).
2. **Domain-refined**: the same checks against inferred value sets.
3. **Local symbolic**: project the spec onto
   W = supp(g₁) ∪ {x} ∪ (offending supports) — drop non-W guard
   conjuncts, keep W-writes. Dropping conjuncts adds behavior, so the
   small system over-approximates the real one on W; run the symbolic
   engine on it and check the condition along in-flight windows.
   Spurious interleavings only make the check conservative — the
   failure mode is a refused rule, never a wrong one. This is the
   22-rule system's SMT-backed behavioral conditions (§4.5 there),
   transplanted to our own engine on a projected "small net".

Experimental mode: gain-freedom means a wrongly-enabled agglomeration
only *loses* projected states, which the projection differential
catches — so optimistic-apply + oracle on small instances is a sound
way to measure which relaxations matter before proving them.

## Process discovery from the transition relation

Petri theory locates processes as S-components. In guarded commands
**every scalar variable is trivially an S-component** (it always holds
exactly one value), so the question shifts: which variables are
*sequencers*? The signal is the discipline the hotbit scan already
recognizes: c read only in pinned equality atoms and written by those
same events defines an automaton A_c (nodes = values, edges = events);
its event group E_c is a "congruent subgroup" of the system's big alt —
a process with program counter c. Then:

* **Local variables** of (c, E_c): touched only by E_c events —
  tensor-local, they descend into that process's subtree of the shape.
* **Mediators**: variables write-then-tested across two groups — these
  ARE the (x, m) agglomeration candidates. Discovery scan and candidate
  scan are one analysis read from two sides: intra-group A_c edges are
  sequential chains (a silent path in A_c = a chain of handshakes —
  resolves the "multiple mediating values" question), cross-group
  mediator edges are synchronizations (keep, or fuse when silent).
* **Shape emission**: one subtree per process, mediators at the join;
  quality metric = fraction of events local to one subtree — the same
  objective reorder-force/Louvain optimize. Louvain is the undirected
  heuristic; the control-variable scan is its directed, semantically
  grounded refinement.
* **Don't rediscover what importers know**: DVE has processes, NUPN has
  units — dve2hsc/nupn2hsc should preserve them as shape; discovery is
  for the wild case.
* This answers Laarman's process-less-STR obstacle constructively:
  synthesize the threads from the transition relation, then run
  process-bound TR on them.

Closing the loop with local discharge: the potential space for a
side-condition check is the **control skeleton** — the product of the
involved A_c automata plus mediator domains, data guards weakened away.
Rules checked on the skeleton, applied to the full spec. Risks: guards
mixing control and data (counters) coarsen the skeleton — domains
mitigate; events pinned on two controllers are boundary events
(attribute by modularity); cap the per-check symbolic cost.

## POR in the explicit engine

Independence `e₁ ⊥ e₂` ⟺ writes(e₁) ∩ (reads(e₂) ∪ writes(e₂)) = ∅ (and
symmetrically) — exact on post-chain specs, since `simplify-constants` /
`simplify-arrays` remove the support over-approximation at its source.
The same relation discharges (6b′). Value refinement gives the
enabling/disabling split per guard atom — the same classification as
(6a). Stubborn-set closure is var-mediated through `readers(p)` — no
event×event matrix, honoring the standing constraint. Entry point:
**deadlock detection with stubborn sets** (simplest conditions, no
visibility, no ignoring problem; preserved-deadlock guarantee testable
exhaustively on small instances; the engine's visitor already supports
stop-at-first-hit). Ample/LTL-X with the cycle proviso later — a DFS
strategy earns its place there.

## v1 scope and open questions

* P and C single-clause guarded commands; one (x, m) per round;
  fixpoint with re-analysis.
* Kernel: pre-agglomeration under **(6a) with late fusion** — sound as
  originally stated, no new machinery. (6b)+(6b′) second, post third;
  all three share the independence relation with POR.
* Ordering vs other passes: agglomeration before `hotbit` (the pattern
  detection wants `x == m` conjuncts and `x := m` writes, not their bit
  encodings); after `simplify-constants` / `simplify-arrays` (exact
  supports).
* Open: multiple mediating values (x as a full automaton, agglomerating
  whole silent paths); the recorded-inverse counting oracle (above); the
  symbolic projection oracle (existential quantification over a frontier
  position); partial agglomeration (fuse only the conforming subset of
  P × C, keep the rest — the 22-rule system shows it pays); query
  rewriting through the recorded constraints (the E-transform) instead
  of a fixed V — subsumes the projection and counting oracles at once;
  certifying rule correctness mechanically (Amat names it open even for
  nets; our two-engine differential is the testing half).
