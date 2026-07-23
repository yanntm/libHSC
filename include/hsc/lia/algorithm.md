# lia — representation, normal form, substitution

## 1. Codes and the constant tag

Two code spaces, both 32-bit, one per kind: `iexpr` (integer-valued) and
`bexpr` (boolean-valued). The low bit is a tag:

* **even** — an interned node: `code = id << 1`, `id` from the kind's
  `mem::intern` table. Code 0 is absence, as everywhere.
* **odd** — an immediate, no node behind it:
  * `iexpr`: an inline constant in sign-magnitude — bit 1 the sign, bits
    2..31 the magnitude (30 bits, so |v| < 2^30). The two encodings of zero
    make the spare one a sentinel: `+0` is the constant 0, `-0` (code 3) is
    **⊥**, the undefined expression (`INTNDEF`: array index out of bounds,
    division by zero). A constant with |v| ≥ 2^30 falls back to a `CONST`
    node; the ranges are disjoint, so each value has exactly one code.
  * `bexpr`: three immediates — `false` (1), `true` (3), `⊥` (5).

The tag is the reference's optimization carried over: constants dominate
real expression populations, and they cost neither a node nor a probe.

## 2. Nodes

One node layout per kind: `{kind, count, payload, operands[count]}`, the
operands a trailing array of codes (like `int_set`). `payload` holds the
variable index (`VAR`) or the value of a wide `CONST`. An `ARRAY` node's
operands are its index expression followed by the raw frontier position of
each cell (the placement paragraph of §4).

Int kinds: `CONST VAR PLUS MULT MINUS DIV MOD POW BITAND BITOR BITXOR
BITCOMP LSHIFT RSHIFT ARRAY WRAPBOOL`. Bool kinds:
`AND OR NOT EQ NEQ LT LEQ GT GEQ`. A variable is a *frontier position*; the
named world ends at the surface.

## 3. The normal form (established by construction)

Factories return normalized codes; there is no separate `eval`. The rules,
applied bottom-up at each construction:

* **⊥ propagates**: any operand ⊥ makes the result ⊥ (`NOT ⊥` is ⊥, an
  `AND`/`OR` with a ⊥ operand is ⊥ — GAL's semantics: undefined is not
  absorbed, it poisons).
* **Constants fold**: every operator on all-constant operands becomes a
  constant (overflow-checked, loud); comparisons become `true`/`false`.
  `DIV`/`MOD` by zero, and `POW` with negative exponent, fold to ⊥.
* **N-ary `PLUS`/`MULT`, `AND`/`OR` flatten**: same-kind operands are
  spliced in; constant operands merge through the neutral element (0, 1,
  `true`, `false` resp.); an absorbing constant (`0` for `MULT`, `false`
  for `AND`, `true` for `OR`) collapses the node; operands are kept
  **sorted by code**, duplicates kept for `PLUS`/`MULT` (x+x), dropped for
  `AND`/`OR` (idempotent); a single survivor is returned bare.
* **`ARRAY` with a constant index** folds to the addressed cell's variable
  (or ⊥ if out of bounds).
* **`NOT NOT e`** is `e`; `NOT` of a comparison is the flipped comparison
  (`NOT (x < y)` is `x >= y`), so `NOT` nodes survive only above `AND`/`OR`.
  `push_negations` drives them through (De Morgan) on demand.

Canonical orientation of comparisons (constant on the right, positions
ordered) is *not* imposed here: `EQ`/`NEQ` operands are sorted by code like
any commutative pair, and the ordered comparisons stay as written — `x < y`
and `y > x` are distinct codes with one meaning. A layer that wants them
identified (a cache key) normalizes at its own seam. [Deviation from
nothing: GAL had no such rule either.]

## 4. Substitution (currying)

`subst(e, x, v)` — replace position `x` by expression `v`, renormalizing
bottom-up through the factories; the result is again canonical. This is the
case bracket's residual step: split the head coordinate, substitute the
class value, and what remains is the residual criterion for the tail.
An assignment's right-hand side grounds the same way. `ARRAY` substitution
reaches the *index* expression only; the cell placements are data, and the
fold to a plain variable on a grounded index is what makes cells ordinary
coordinates afterwards.

`support(e)` reports the positions an expression reads — what decides at
which cut a criterion crosses. `first_subexpr` peels the innermost nested
expression (an `ARRAY` index) so indirection resolves innermost-first:
`tab[tab[x]]` curries `x`, then the inner access, then the outer.

An `ARRAY` node **carries its placement**: operand 0 is the index
expression, operands 1… the frontier position of each cell in index order —
spread or permuted freely, nothing assumes adjacency, and the positions are
raw data every walker skips (only the index is an expression). A constant
index folds to the cell's variable (⊥ out of bounds), so a variable is the
degenerate array access and a held node always has an open index.
`array_positions` reports the cells a criterion is pinned above while its
index is open; `shift_positions` re-roots by shifting scalar positions and
cell placements together. Sharing between isomorphic components survives
exactly up to that translation: same relative layout, same code.

## 5. What is deliberately absent

Reindexing under shape restructuring is done by `subst` of variables for
variables. No environments, no names, no GC hooks: codes are permanent for
the session, like every interned value in libHSC (`mem/` owns reclamation
policy). No preimage machinery: expressions are read and curried, never
inverted here.
