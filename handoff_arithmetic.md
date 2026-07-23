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

1. **Crossing criteria and updates on `lia` expressions** — `(< a (+ b c))`,
   `x := y + z`: generalize `select_compare`'s residual to a curried `bexpr`
   (cacheable by its code), events gain expression actions. **Settled shape
   of the do-language** (GAL's statement language, staged):
   `STMT ::= (:= x e) | (ite COND STMT* [STMT*])`, sequence = repeated
   `(do …)` (compose); `when` is the sole *refusing* construct, so deadlock
   and enabling stay cheap per-position filters. An `ite` whose condition
   crosses leaves IS the §7 case bracket — one machinery for both. GAL's
   `abort` returns the algebra's 0 — absence is a legal answer everywhere
   (Discipline 1), `keep_if(bfalse)` is that term today, composition
   propagates it; no new machinery. Only the deadlock check assumes refusal
   sits in `when` (enabling as a pre-state filter) — revisit that one
   consumer when abort-bearing events arrive.
2. **`tab[i]`** — indirection through `first_subexpr`/`subst_cell`, already
   exercised at the expression level; `tab[tab[x]]` is the remaining half
   of the R4 gate.
3. **BEEM corpus greening** — `samples/divine/status.tsv` is the
   scoreboard: 10 run today, 150 §7-refusals and 114 unread expression
   atoms, both fed by item 1. Then: DVE `byte` wraps mod 256 (a wrapping
   shift in the theory, not a bound — anderson et al. diverge without it);
   map the `.prop1.reach` criteria to `select`; check the run-ok counts
   against libITS baselines (`~/git/libITS/perfs/test_models`, bounded or
   skipped). `trans_*` transient states are noted per file, semantics not
   reproduced.

## What stands under this (validated, documented in the packages)

The separable engine and the first §7 shape are done: `split_equiv` on the
leaf and on diagrams, `select_compare` at any depth and shape, surface
`select`, symbolic (`lia`) guards end to end with Int leaves and opt-in
bounds, compose-based multi-step events, the DVE front end (276/276 parse
and transform), the Angiogenesis-PT-05 flat-vs-Louvain cross-check. Details:
`include/hsc/{lia,dve,surface}/`, `hsc/query.hh`, `examples/models/`,
`samples/divine/`, git.

**Gate (roadmap R4):** the `x < y`-across-a-cut half is met, including the
decomposed-net stress; `tab[tab[x]]` vs a brute-force oracle remains.

## Blockers

None on the critical path — the operational model is settled, expressions
are interned with their normal form, and the separable engine is done.

## Explicitly not now (exists, or secondary to the destination)

Benchmark campaign, ITS-tools/libDDD baselines, the R2 memory wall, the fast FDD
leaf (R3), P-invariant bounds for non-safe StateSpace. All real, all deferred.
