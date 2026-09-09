# `trace/` — algorithms

Self-contained. `R` the reachable set, `next` the sum of the event terms
`tᵢ` (named), `predᵢ` the converse of `tᵢ` relative to `R`
(`core/algorithm.md` §9), `sel_q` a selector.

## 1. A path from a set to a set

Question: a shortest run `s₀ →t₁ s₁ → … →tₖ sₖ` with `s₀ ∈ From`, `sₖ ∈ To`,
every `sᵢ` (i < k) satisfying a constraint `q` (states the run may pass
through; `true` when unconstrained). Two phases, both symbolic.

**Layers, forward.** `L₀ = From`; `Lᵢ₊₁ = next(sel_q(Lᵢ)) ∖ (L₀ ∪ … ∪ Lᵢ)`;
stop at the first `k` with `has_image(sel_To, Lₖ) ≠ 0` (a target reached —
tested existentially), or when a layer is empty (no path). The layers are
kept: they are the distances from `From`, and a state of `Lᵢ` has a
predecessor in `Lᵢ₋₁` by construction.

**Backtrack, one state at a time.** Pick `sₖ`: one word of `Lₖ ∩ To`
(`get-witness`). Then for `i = k … 1`: find an event `tⱼ` and a state
`sᵢ₋₁ ∈ Lᵢ₋₁ ∩ predⱼ({sᵢ})` — the inverted event applied to a single state,
intersected with the previous layer; the first event with a nonempty answer
is the label, one word of the answer the state. Every step is one
application of one inverted event to a one-state diagram: cheap, and the
inverse is exact on `R` so the step is a real transition.

Without the inverse (a case bracket that assigns, refused by the inverter),
the backtrack falls back to a forward test per event: `tⱼ(Lᵢ₋₁) ∩ {sᵢ} ≠ 0`
says the event fires from *some* state of the layer; the state is then
found by splitting `Lᵢ₋₁` — arc by arc, the half whose image still hits
`sᵢ` — which is a binary search over the layer's words (logarithmic in its
size in the number of images computed).

Output: the words `s₀ … sₖ` and the names `t₁ … tₖ`, printed in the surface's
`(word …)` syntax so the explicit engine can replay them.

## 2. A cycle through a set

`EG f` at `s` is witnessed by a path to a **lasso**: a run inside `Sat f`
from `s` to a state `h` of the hull `gfp(pred)·Sat f` (every state of the
hull has a successor in the hull, so a cycle is reachable from `h` inside
it), then a cycle: a path from `next({h}) ∩ hull` back to `{h}` through the
hull. When the deadlock-terminated branch applies (`dead ∩ Sat f` nonempty
and reachable through `f`), the witness is the path to a dead `f`-state and
no cycle.

## 3. The witness tree of a forward form

A `TRUE` verdict of `I ∧ φ ≠ ∅` (or a `FALSE` one, which is the witness of
`I ∧ ¬φ`) is explained by reading the set expression of the `nonempty?` leaf
that answered, outside in — each operator adds a segment:

| set expression | segment |
|---|---|
| `init` | the initial state |
| `filter(r, f)` | a state of `[r]` satisfying `f` (one word), and the witness of `r` ending there |
| `ey(r)` | one step: a state of `[r]` and the event to the chosen state |
| `fwdu(r, q)` | a path through `q`-states from a state of `[r]` to the chosen state (§1 with `From = [r]`, `To = {chosen}`) |
| `fwdg(r, q)` | a path from `[r]` through `q` to a state with an infinite `q`-path, then its lasso (§2) |
| `restrict(r, φ)` | a state of `[r] ∩ Sat(φ)`, the witness of `r` ending there, then the **backward** explanation of `φ` at that state |

Backward explanation of `φ` at `s` (the `Sat` rules of `ctl/algorithm.md`
§3): `EX f` one step to a state of `Sat f`; `E[f U g]` a path through
`Sat f` to `Sat g` (§1); `EG f` §2; `f ∧ g` both; `f ∨ g` the child that
holds; a universal operator holds by exhaustion and is reported as such,
with no path (`its-ctl`'s "formula holds on all paths"); a state formula is
its own witness.

The choices — which target state, which predecessor, which disjunct — are
the first found; the tree is *a* witness, not the shortest overall (each
segment is shortest for its own endpoints).

## 4. Cost

Layers cost a forward closure's worth of images (one per layer, on sets no
larger than `R`); the backtrack costs `k × |events|` one-state
applications at most. The witness tree adds one search per segment. All of
it after the verdict, on demand.
