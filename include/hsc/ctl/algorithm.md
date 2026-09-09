# `ctl/` — algorithms

Self-contained: every rule the code implements is stated here, in the
calculus's vocabulary. Notation: `R` the reachable set, `I` the seed,
`next` the sum of the forward event terms, `pred` the sum of the inverted
ones, `sel_f` the selector term of a state formula `f` (the guard-only event
keeping the states where `f` holds), `Sat(φ) ⊆ R` the states satisfying
`φ`. Set operations are the calculus's `join / meet / minus`; `h(S)` is
term application; `lfp(h)·S` is the least fixpoint of `X ↦ X ∪ h(X)` above
`S` (the `reach` of the surface, saturated), `gfp(h)·S` the greatest
fixpoint of `X ↦ X ∩ h(X)` below `S` (`core/algorithm.md` §8).

## 1. Semantics

CTL is evaluated at the initial states over the reachability graph. A
**deadlock ends its path** (the contest's reading, which `its-ctl` and the
oracles share): a maximal finite path is a path, so `EG f` holds at a
deadlocked `f`-state, `EX f` is false at a deadlock and `AX f` true there,
and `A[f U g]` fails at a deadlock where `g` fails. `dead` denotes the
reachable deadlocks; it is a state predicate (`¬(g₁ ∨ … ∨ gₙ)` over the
event guards), hence a selector, hence exact on data with no preimage
(`core/algorithm.md`, and the paper's 4.10).

## 2. The formula DAG

    φ ::= atom | true | false | ¬φ | φ ∧ … ∧ φ | φ ∨ … ∨ φ
        | EX φ | AX φ | EF φ | AF φ | EG φ | AG φ
        | E[φ U φ] | A[φ U φ] | E[φ W φ] | A[φ W φ]

Nodes are hash-consed: `(op, atom, kids)` interned, so equal subformulas are
one node and every memo below is keyed by node id. Atoms are opaque
indices; the caller says which selector each denotes. The deadlock
predicate is an atom like any other.

**Negation normal form.** `¬` is pushed to the atoms by the duals:
`¬EX f = AX ¬f`, `¬EF f = AG ¬f`, `¬EG f = AF ¬f` and conversely;
`¬E[f U g] = A[¬g W (¬f ∧ ¬g)]`, `¬A[f U g] = E[¬g W (¬f ∧ ¬g)]`,
`¬E[f W g] = A[¬g U (¬f ∧ ¬g)]`, `¬A[f W g] = E[¬g U (¬f ∧ ¬g)]`; de Morgan
on `∧ / ∨`; constants fold. A **state formula** is a node without path
operator; the checker treats it as one selector whatever its shape.

**Existential form.** Every universal operator is a negated existential
one: `AX f = ¬EX ¬f`, `AG f = ¬E[true U ¬f]`, `AF f = ¬EG ¬f`,
`A[f U g] = ¬(E[¬g U (¬f ∧ ¬g)] ∨ EG ¬g)`, `A[f W g] = ¬E[¬g U (¬f ∧ ¬g)]`,
`E[f W g] = E[f U g] ∨ EG f`, `EF f = E[true U f]`. The evaluation rules
below are written for `EX`, `EU`, `EG`, `¬`, `∧`, `∨`; a universal node is
evaluated through this expansion, with `¬X = R ∖ X`.

## 3. Backward evaluation (`Sat`)

For a node `φ`, memoised:

| φ | Sat(φ) |
|---|---|
| state formula `f` | `sel_f(R)` |
| `¬f` | `R ∖ Sat(f)` |
| `f ∧ g`, `f ∨ g` | `meet`, `join` of the children |
| `EX f` | `pred(Sat f)` |
| `E[f U g]` | `lfp(sel_f ∘ pred)·Sat(g)` — the filter after the step; when `f` is not a state formula, `lfp(pred)` with `meet(·, Sat f)` after each step |
| `EG f` | `D ∪ lfp(sel_f ∘ pred)·D ∪ gfp(pred)·Sat(f)` with `D = dead ∩ Sat(f)` |

The `gfp` term is the states of `Sat(f)` with an infinite `f`-path; the two
lfp terms add the `f`-paths that end in a deadlock. When the graph is
acyclic (`gfp(next)·R = ∅`, computed once per model) the `gfp` term is
dropped. Every rule needs `pred`: this table is what the inverse buys
(`core/algorithm.md` §9), and a model without inverted events refuses these
nodes rather than approximating.

**The `gfp` by frontier.** One full image of a big set costs about as much
as its saturation did (the step is every event applied to every node,
joined at every level), and the round form `X ← X ∩ h(X)` pays it once per
round. A state can only drop out of `X` when it has lost its last
`h`-witness, and its witnesses were in the states just removed; so after
the first full image the work is proportional to what moved:

    gfp_frontier(h = Σ_e h_e, X0):
      X = X0
      D = X ∖ h(X)                               # one full image
      while D ≠ ∅:
        X = X ∖ D
        C = h(D) ∩ X                             # the candidates: they had a witness in D
        K = ⋃_e ( h_e( h_e⁻¹(C) ∩ X ) ∩ C )       # those that still have one in X
        D = C ∖ K
      return X

`h_e⁻¹` is the converse of the component `h_e`: for `h = pred` it is the
forward event, for `h = next` the inverted one. The converses must be
exact as relations on `R` (the inverted events are, protected or not:
what protection removes lies outside `R`, and `C`, `X` lie inside).
Every `D_i` is exactly `X_i ∖ h(X_i)`: a state of `X_{i+1}` with no witness
left had one in `X_i`, hence in `D_i = X_i ∖ X_{i+1}`, hence is in
`h(D_i)`; and `K` is `h(X_{i+1}) ∩ C` by the converse. The fixpoint is the
same as the round form's. Measured on the MCC corpus (588 runs, 60 s) the
round form answers more — 6414 against 6323, 31 runs down and 10 up for
the frontier — so the **round form is the default** and the frontier form
is the variation point `HSC_CTL_GFP=frontier`: the removed slice is often
large, and then the frontier's per-event converse images cost more than the
one image they replace.
Used wherever the checker takes a hull: the `EG` rule, `fwdg`, the acyclicity
test, and the witness hull.

A hull needs no protection (`core/algorithm.md` §9): every round meets with
`X ⊆ R`, so a converse composed with `within(R)` would only add one meet
with `R` per event per round — on a net where every event is protected
(TokenRing: 1111 of 1111) that is the round's whole cost. The hull therefore
takes the **raw** converses when the model offers them (`raw_pred_events`),
the protected ones otherwise; the lfp closures keep the protected ones,
where the protection is what keeps them inside `R`.

## 4. Forward form

The question is `I ∧ φ ≠ ∅` (or its dual, see polarity). The conversion
rewrites the pair `(r, φ)` — read `r ∧ φ` with `r` a **set expression** and
`φ` a formula — pushing the seed inward so that the outermost path
operators become images and forward closures. Set expressions:

    r ::= init | filter(r, f)            f a state formula: sel_f(r)
        | ey(r)                           next(r)
        | fwdu(r, q)                      lfp(next ∘ sel_q)·r, or lfp(next) with meet(·, Sat q) per step
        | fwdg(r, q)                      see below
        | restrict(r, φ)                  r ∩ Sat(φ), φ evaluated backward

`fwdu(r, q)` is `lfp Z[r ∨ EY(Z ∧ q)]`: the states reached from `r` through
`q`-states, `r` itself included whatever `q`. `fwdg(r, q)` is nonempty iff
some state of `r` satisfies `EG q`:

    reach_q = filter(fwdu(filter(r, q), q), q)
    fwdg(r, q) = gfp(next)·reach_q  ∪  (dead ∩ reach_q)

(`EH(Y) = gfp Z[Y ∧ EY(Z)]`, the states of `Y` reached from a cycle inside
`Y`; the second term is the deadlock-terminated `q`-paths.)

**Rules**, on `(r, φ)` with φ in existential form, `⊕` the combinator of
the polarity (§5). `qf(f)` means `f` is a state formula.

    (r, EX f)          → (ey(r), f)
    (r, E[q U f])      → (fwdu(r, q), f)
    (r, EG q)          → nonempty?( fwdg(r, q) )                       terminal
    (r, ¬¬f)           → (r, f)
    (r, f ∧ g)         → (filter(r, f), g)      if qf(f); else (restrict(r, f), g): the right conjunct continues forward, the left restricts
    (r, f ∨ g)         → (r, f) ⊕ (r, g)
    (r, ¬(f ∨ g))      → (filter(r, ¬f), ¬g)    f the state-formula (or non-convertible) child
    (r, ¬(f ∧ g))      → (r, ¬f) ⊕ (r, ¬g)
    (r, f) with qf(f)  → nonempty?( filter(r, f) )                     terminal
    (r, φ) otherwise   → nonempty?( restrict(r, φ) )                   terminal

"Otherwise" is a negated path operator under `r` — a universal node: its
`Sat` is computed backward (§3) and intersected with the forward set. A node
is **convertible** when it never puts a negation over a path operator:
state formulas, `EX/EU/EG` nodes, `¬¬`-pairs of those, and `∧ / ∨` of
convertibles.

The output is a **question tree**: `nonempty?(r)` leaves under `any`
nodes.

**A choice to re-examine.** In `f ∧ g` with both conjuncts temporal, one
continues forward and the other is evaluated backward; nothing in the
semantics prefers either. VIS sends the right one forward, and so does this
conversion (the last conjunct in written order, i.e. the highest node id).
`HSC_CTL_FWD=left` sends the first one instead — a variation point.
The path operators offer no such choice: `E[q U f]` always carries the seed
through `q` and checks `f` at the end.

## 5. Polarity

With one initial state, `I ⊨ φ` iff `I ∧ φ ≠ ∅` iff `I ∧ ¬φ = ∅`. The
conversion picks the side whose top is convertible: if `φ` is convertible,
ask `I ∧ φ ≠ ∅`; else run the rules on `¬φ` (in normal form), asking
`I ∧ ¬φ ≠ ∅`, and take the **complement** of the answer. Either way the
tree is `any` over `nonempty?` leaves (`⊕ = any`; VIS's `all` over `empty?`
leaves is the same tree read through De Morgan) and the form records
whether the verdict is negated. This is VIS's `compareValue`. The MCC seed
is a single marking; a seed with several states always asks the negated
question (all initial states must satisfy φ).

## 6. Evaluation order, memo, and the existential leaves

Set expressions and `Sat` nodes are interned like formulas and evaluated
once. `any` stops at the first nonempty child — the existential
short-circuit at the tree level.

A `nonempty?` leaf does not need the set it names, only whether it is empty,
and its outermost operator is answered **existentially** (`core/algorithm.md`
§10, `has_image`), the operand below it built in full:

    nonempty?(init)            = I ≠ ∅
    nonempty?(filter(r, f))    = has_image(sel_f, [r]) ≠ 0
    nonempty?(ey(r))           = has_image(next, [r]) ≠ 0
    nonempty?(fwdu(r, q))      = [r] ≠ ∅                     the closure contains its seed
    nonempty?(fwdg(r, q))      = reach_q ≠ ∅ and (dead ∩ reach_q ≠ ∅ or has_image(gfp(next), reach_q) ≠ 0)
    nonempty?(restrict(r, φ))  = [r] ∩ Sat(φ) ≠ ∅

with `[r]` the full evaluation of `r`. Backward, `EG` asks
`has_image(gfp(pred), Sat f)` before paying the hull, and skips it when there
is no cycle; `has_cycles` is `has_image(gfp(next), R)`. The witness subsets
are never memoised as the value of a set expression — the two readings keep
separate tables.

**On the fly.** A leaf `nonempty?(filter(fwdu(r, q), f))` whose constraint
`q` is temporal has a closure that is breadth-first anyway (its constraint
is data, `Sat q`); instead of closing it and then filtering, the search
tests each new frontier for `f` and stops at the first hit:

    seen := frontier := [r]
    loop: has_image(sel_f, frontier) ≠ 0 → yes
          frontier := next(frontier ∩ Sat q) ∖ seen;  empty → no
          seen := seen ∪ frontier

the emptiness check of a construction done while constructing it. A
state-formula constraint keeps the saturated closure, which is cheap.

**Deadlines.** Every iteration loop consults the manager's interrupt hook
once per round; a property stopped by its deadline answers `TIMEOUT`, and
what it memoised (sets, `Sat`, closures) stays for the next attempt. The
`hsc-pn` driver asks the CTL properties in rounds of growing budget so the
cheap ones are answered before an expensive one can take the whole budget.

**Variation points.** `HSC_CTL_EXIST=0` turns the existential leaves off
(every set in full); `HSC_CTL_OTF=0` turns the on-the-fly search off;
`HSC_CTL_SCCFAST=1` turns on the per-arc cycle witness inside
`has_image(gfp)` (off by default: measured to lose where no component
cycles on its own); `HSC_CTL_PROTECT=test|never|always` decides which inverted events are
intersected with `R` after each step (`never` is sound for verdicts at the
seed — see `research_notes/invert.md` §3 — and keeps every backward closure
a saturating one, at the price of spurious states carried);
`HSC_CTL_GFP=frontier` takes every hull by the frontier form instead of
the round form (§3).

**Observation point.** `HSC_CTL_TRACE=1` prints, on stderr, the wall time
of every `sat`, `eval` and `nonempty` scope, and the number of `gfp`
rounds the diagram engine ran inside it (a round is one image and one
meet). On the MCC nets tried, an `EG` costs 2 to 9 rounds: the closure is
bound by the cost of one full image over the big set, not by its depth.

The `pred`-free fragment — everything the rules leave as `ey / fwdu / fwdg
/ filter` — needs `next`, the selectors, `dead`, `lfp` and `gfp` only. The
inverted events are asked for lazily, the first time a backward operator
needs them; a model that cannot provide them refuses those nodes. A model
may offer the raw converses beside the protected ones (§3, the hull).

## 7. Verdict

`TRUE` / `FALSE` at the seed, with the technique tags of the driver. A
refused node makes the verdict `unknown`, reported as such: never a guess.
