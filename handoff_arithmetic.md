# Handoff — cross-level arithmetic

Current state and the next objective. Rewritten in place; no history here.

## The objective

**Arithmetic across a cut.** Criteria and updates relating coordinates on
different sides of a cut — `a < b + c`, `x := y + z`, `x < y`, `tab[i]`. The
inseparable fragment (hsc_core5 §7): what does not factor into per-position
parts, and so forces `split_equiv`. Ours and not libITS's; the whole point.

Everything below the cut is finished and validated: shapes, diagrams, the set
algebra, product terms, saturation, the surface, NUPN import with Louvain
decomposition, the MCC examinations checked against the oracle. The surface
already parses and rejects crossing forms with a §7 pointer — the seam is cut,
nothing before it is in the way.

## The operational model (now settled — build to this, not the Python)

The Python prototype is **not** a reference. Build from the calculus and this:

- **`split_equiv(code, expr)` → `{ LIA value → partition-piece }`.** Partition a
  code by the integer value `expr` takes on it; **exact**, one class per distinct
  value — no cost/soundness knob, this is added expressiveness. A leaf evaluates
  `expr` over its coordinate and groups. The label lives in the interchange (LIA)
  theory.
- **Crossing = currying deeper.** `a.x < b.y`: split `a` by `x` → `{xv → piece}`;
  carry the residual `xv < b.y` into `b`; split `b` by `y`; `xv < yv` is now
  ground → keep/drop. Descend each named side to its coordinate, substitute,
  recurse until the LIA formula is ground.
- **A node's `split_equiv` is the bracket one level in** — same act, deeper.
- **Federation = merge by common key** (the residual value). No kernel machinery.
- **A case is an ordinary `G` event under saturation**: it unwraps (query/split),
  rewraps (rebuild the diagram), hands back; re-saturation proceeds as for any
  `G`. No special schedule, no separate termination story.
- **Assign is the same machinery**: `x := y + z` curries the operands to a ground
  value, then writes it.

## Names survive restructuring for free

Expressions are written on **flat** place names (`x`, `y`). Events are
name-keyed, and the shape declaration is the single place a name is bound to a
position. So Louvain (or any) restructuring cannot lose a reference: the same
`x < y` resolves through whatever shape — a within-unit comparison if `x`,`y`
land together, a deeper case if they split across units. The nested path
`a.b.x` is a recoverable view of the resolution, not a rewrite of the criterion.
Decomposition *creates* the cross-level structure that exercises `split_equiv`;
it never invalidates an expression.

## Engineering queue (next action first)

1. **`split_equiv` on the `int_set` leaf** — partition a code by the value of a
   linear expression `coeff*x + offset`; overflow-checked. *In progress.*
2. **The LIA interchange theory** — expressions over positions, substitution of a
   coordinate by a value (currying), a residual normal form, interning.
3. **The case bracket** at a cut — split the head coordinate, carry residuals
   into the tail, ground-evaluate; assemble the resulting diagram. Comparison of
   two places is the first shape (`x < y` = `Σ_v [x=v] ⊗ [y > v]`, the tail
   restriction a plain meet).
4. **`split_equiv` on diagrams** — the node instance of the bracket, one level
   in; merge pieces by common residual key.
5. **Surface**: accept crossing criteria (`(< x y)`, `(< a (+ b c))`,
   `x := y + z`), resolve flat names through the shape, compile to the bracket.

**Gate (roadmap R4):** `x < y` across a cut resolving at the surface, and
`tab[tab[x]]`, each agreeing with a brute-force oracle over a tiny carrier.
Stress it by decomposing a flat net so the two places land in different units.

## Blockers

None on the critical path — the operational model is settled and the separable
engine is done. Open only in the tail: `tab[i]` indirection representation, and
the residual normal form's interning, both settled in code as they arise.

## Explicitly not now (exists, or secondary to the destination)

Benchmark campaign, ITS-tools/libDDD baselines, the R2 memory wall, the fast FDD
leaf (R3), P-invariant bounds for non-safe StateSpace. All real, all deferred.
