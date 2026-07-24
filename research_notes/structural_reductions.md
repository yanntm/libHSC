# Structural reductions in hsc — a working note

Status: idea note, pre-spec. Grew out of the rewrite-chain work (the
chain, its passes, and the two-engine differential are in place; see
`include/hsc/surface/algorithm.md` §1c and `handoff_explicit.md`). The
goal here: agglomeration (Berthelot pre/post, Lipton transaction
reduction) and partial-order reduction, re-expressed over guarded
commands with the analyses we already compute — supports (ctrl / reads /
writes per event, sorted sparse sets, `readers(p)` inverse) and inferred
value domains.

## The contract

Fix a set **V of variables of interest**. A reduction is correct when
the **projection of the reachable set onto V is unchanged**; runs may
gain or lose hidden-only stutter steps, so the guarantee extends to
stutter-insensitive LTL over V. Counts are *not* preserved (that is the
point), so the `reach == xreach` differential does not test these
passes. The oracle is the **projection differential**: hide the
non-observed coordinates and compare projected reachable sets, full vs
reduced — in the explicit engine a strip-and-dedup, symbolically an
existential quantification (to build).

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

### Pre-agglomeration criteria (producer silent, delayed)

1. **Hiding**: x ∉ V.
2. **Silent producer**: writes(t₁) = {x} exactly. Stronger than the net
   condition (h consumes its inputs) but it buys the key property: the
   intermediate state projects on V like its predecessor, so deleting it
   loses no V-point even when a run dies there — pre-agglomeration then
   needs **no liveness side condition** for projected reachability.
3. **Exclusive mediation**: nobody outside C can see m. Strong form: no
   other event reads x. Value-refined: other readers are fine if every
   guard atom on x excludes m (`== k`, k ≠ m; `!= m`); a value-read of x
   is fatal.
4. **Consumers exit**: each t₂ leaves x ≠ m.
5. **No initial marking**: seeds have x ≠ m.
6. **Producer persistence — the condition the net gets free and we pay
   for.** In a net, h consumes its preconditions at its own instant; in
   the fusion g₁ is evaluated late, at the consumer's instant, and an
   interleaved event may have disabled it — the original run exists, the
   fused one does not. Two static discharges, either suffices:
   * **(6a)** no third event can *disable* g₁: per atom of g₁ over a
     variable some event writes, every constant write satisfies the
     atom (enabling writes are harmless — they only add fused runs that
     existed as reorderings). This is the value-sensitive enable/disable
     classification, the same one POR wants.
   * **(6b)** trivial consumer: g₂ is exactly `x == m` — then fuse
     eagerly at the producer's instant and late evaluation never
     happens. (Berthelot's "F has no other precondition".)

### Post-agglomeration (dual)

Producer may be visible; consumers must be **silent**
(writes ∩ V = ∅) and **(6b) is mandatory** — a consumer with a residual
guard means the original can strand in the intermediate (even deadlock
there), which post-fusion erases. The classic progress caveat,
discharged statically.

### Mapping to the Petri structural conditions

token-in-place ↦ value-in-variable; `•p = {h}` ↦ P as computed;
"no other transition tests p" ↦ the value-refined reader scan;
`h• = {p}` ↦ writes(t₁) = {x}; token consumption at h's instant ↦ the
explicit persistence-or-trivial-guard disjunction (6). Every check is a
support intersection or an atom-vs-constant-write comparison.

## POR in the explicit engine

Independence `e₁ ⊥ e₂` ⟺ writes(e₁) ∩ (reads(e₂) ∪ writes(e₂)) = ∅ (and
symmetrically) — exact on post-chain specs, since `simplify-constants` /
`simplify-arrays` remove the support over-approximation at its source.
Value refinement gives the enabling/disabling split per guard atom.
Stubborn-set closure is var-mediated through `readers(p)` — no
event×event matrix, honoring the standing constraint. Entry point:
**deadlock detection with stubborn sets** (simplest conditions, no
visibility, no ignoring problem; preserved-deadlock guarantee testable
exhaustively on small instances; the engine's visitor already supports
stop-at-first-hit). Ample/LTL-X with the cycle proviso later — a DFS
strategy earns its place there.

## v1 scope and open questions

* P and C single-clause guarded commands; one (x, m) per round;
  fixpoint with re-analysis.
* Start with pre-agglomeration under (6b) — the smallest bulletproof
  kernel — then add (6a).
* Open: relaxing (2) to hidden-only writes (needs the intermediate's
  projection argument through *chains* of hidden writes); multiple
  mediating values (x as a full automaton, agglomerating whole silent
  paths); interaction with hotbit (the encodings commute with the
  reductions?); the symbolic projection oracle (existential
  quantification over a frontier position); deadlock-over-V as an
  optional preserved property (forces (6b) everywhere).
