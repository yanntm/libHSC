# A CTL checker in HSC — where we stand, what is missing, how to get there

Discussion note, no code. The question: how far is libHSC from a symbolic CTL
engine we can run on the MCC `CTLCardinality` / `CTLFireability`
examinations, measured against `its-ctl` (libITS `CTL/`, the VIS front end
plus forward CTL on libDDD). Sources read: `include/hsc/core/*`,
`hsc/event.hh`, `hsc/query.hh`, `leaves/int_set.hh`, the surface manual §5/§8,
`tools/pn_solver.hh`, the vendored `petri/expr/CtlFormula.h` +
`CtlSimplify.h`; libITS `CTL/src/mc/ctlCheck.cpp`, `ctlp/ctlpUtil.c` (the
forward conversion); libDDD `SHom.cpp` (`fixpoint`, `Inter`, `has_image`,
`invert`, the selector/commutation rewrite); PetriSpot `CTL_PLAN.md` and
`Petri/src/ctl/algorithm.md`.

## 1. Short answer

Everything *around* the engine exists: the MCC CTL parser and its negation
normal form are already vendored, `hsc-pn` already drives a surface session
per property, atoms compile to selector terms (Cardinality and Fireability
both), the deadlock predicate is exact without a preimage, and forward
reachability under a constraint is expressible today as `reach` from a seed
over a filtered event term. What is missing is exactly four pieces of the
term algebra, all of which the reference engine leans on and none of which
the calculus currently spells:

| piece | libDDD/libITS name | HSC status |
|---|---|---|
| existential image test | `has_image` | absent — every non-emptiness is a full `select` + `count` |
| inverse relation, relative to a context | `invert(pot)`, `getPredRel(reach)` | absent by design ("theory exports no preimage", paper §2 out-of-scope) |
| meet on terms, deflationary closure | `h * id`, `fixpoint(h * id)` | absent — noted open in the paper's future work (item 7) |
| saturation under a constraint | the `fixpoint((sel & ΣF) + id)` commutation rewrite | absent, but largely subsumed by "composition of products is a product" (§4.4) |

Plus a fifth that is not core: the checker itself (formula walk, Sat memo,
forward rewrite, existential short-circuit) and the `hsc-pn` plumbing.

Forward CTL is the route that avoids the inverse entirely on the top
fragment; it needs only the first, third and fifth pieces. That is the order
to build in.

## 2. What the reference engine actually uses

`CTLChecker` (libITS) works on two caches per subformula: a **homomorphism**
when the subformula is a state formula (a selector: atomic predicates,
booleans over them, `id`/`null` for true/false; anything temporal yields the
sentinel `stop`), and a **state set** otherwise. Its primitives, in libDDD
notation (`+` union, `&` composition, `*` intersection with a selector or a
constant set):

* `EX f` = `pred(Sat f)`; `EY f` = `next(Sat f)`.
* `E[f U g]` = `fixpoint((sel_f & pred) + id)(Sat g)` when `f` is a selector,
  `fixpoint((Sat_f * pred) + id)(Sat g)` otherwise — a constrained lfp,
  saturated (`is_top_level = true`).
* `EG f` = `dead_f ∪ lfp-back-through-f(dead_f) ∪ fixpoint(pred * id)(Sat f)`,
  where `dead_f` = reachable deadlocks satisfying `f`. The last term is a
  **gfp** (`X ↦ X ∩ pred(X)`); libDDD iterates it naively at the top level.
  Before paying it: `hasSCCs()` once per model — `fixpoint(next * id).has_image(reach)`;
  if the reachability graph is acyclic the gfp term is dropped altogether.
* Forward form (VIS `Ctlp_FormulaArrayConvertToForward`), applied to
  `Init ∧ φ` with φ in existential form: `p ∧ EX f → EY(p) ∧ f`;
  `p ∧ E[q U f] → FwdU(p,q) ∧ f` with `FwdU(p,q) = fixpoint((next & sel_q) + id)(p)`;
  `p ∧ EG q → FwdG(p,q) = fixpoint(next * id)(Reach(p,q)) ∪ (dead ∩ Reach(p,q))`
  with `Reach(p,q) = fixpoint((sel_q & next)+id)(p ∩ q)`; `Cmp` nodes turn a
  set into a truth value (emptiness test). Every operator here is `next`
  and a gfp: **no inverse**.
* The **existential short-circuit** (`need_exact = false`, only the top of
  the tree, propagating down through `OR`, `EX`, `EU`, `EG`): the answer is a
  nonempty witness subset or empty, never the full set. `ID`: `sel.has_image(reach)`;
  `EU`: `has_image` of the *goal* alone (the goal is included in the
  fixpoint); `EG`: deadlocks first, then `fixpoint(pred * id).has_image(Sat f)`.
  `has_image` is not a boolean: it returns a nonempty *subset* of the image
  (libDDD's default is the full image; `Add` returns the first summand's
  nonempty image, `Fixpoint` has the "fast SCC detection" below, `Compose`
  tries `left.has_image(right.has_image(d))` and falls back to the full
  image when that is empty).
* **The inverse relation, with context.** `ITSModel::getPredRel(reach)`:
  `rel = next.invert(reach)`; if `rel(reach) ⊆ reach` the raw inverse is
  used ("exact"); otherwise, per named transition, those whose inverse stays
  inside `reach` are kept raw and the offenders are protected as
  `t.invert(reach) * reach`. The context is needed because the inverse of a
  non-injective local hom (`x := c`) is "from `c` to every value of the
  domain" and the domain is only known from the potential; for a composite
  SDD hom the per-component potentials are projections, whose product
  over-approximates `reach` — hence the exactness test.
* **Saturation under a constraint — the hidden rule.** `fixpoint(h, top)` in
  `SHom.cpp` pattern-matches `(sel & ΣF) + id` (or `(ΣF & sel) + id`), splits
  `F` into `doC` (summands whose variable range is disjoint from the
  selector's, or that are selectors themselves: they **commute** with `sel`)
  and `notC`, and rewrites to

      fixpoint( id + fixpoint((sel & Σ notC) + id) + sel & fixpoint(Σ doC + id) )

  so the commuting events saturate unconstrained and are filtered once
  (filtering every step equals filtering at the end when the filter commutes
  and is idempotent), and only the non-commuting ones carry the filter
  through the iteration. The inner `fixpoint(Σ doC + id)` is a plain
  saturation. This is the one element that keeps constrained fixpoints from
  degrading to breadth-first.
* **Fast SCC detection** (`Fixpoint::has_image` on `(ΣF) * id`): push
  `fixpoint(F_below * id)` to the sub-diagrams — a cycle found using only
  events below the cut is a real cycle of the whole; also the local `L`
  part. A *sufficient* witness for `hasSCCs` and `EG`, never a proof of
  absence; on miss, fall back to the full gfp.

## 3. Inventory: what HSC already has

* **Set algebra** `join / meet / minus`; emptiness and equality are integer
  comparisons. `R ∖ X` is the negation relative to the reachable set, exact
  (paper Prop. 4.8: complement is relative difference; no top anywhere).
* **Terms** `id | node | sum | compose | lfp | saturate | expr` (op_kind).
  Composition of node terms is componentwise, so **the composition of two
  product events is a product event** — the property §4.4 turns on.
* **Selectors as terms.** `(when BEXP)` is a guard-only event; `apply_atom`
  compiles *any* query atom that way: separable pieces fuse per leaf
  (`keep_if`), crossing pieces (a Cardinality sum `(<= (+ p1 p2) 3)`) become
  case brackets. Selector-hood is a definition of the paper (§4.7). So
  `sel_f ∘ h` and `Sat_f = sel_f(R)` are one and the same object today.
* **Deadlock is exact and needs no preimage**: `deadlock_atom` is
  `(not (or G_1 … G_n))` over the transition guards, evaluated on data
  (paper 4.10). libITS computes it as `reach − invert(next)(reach)`.
* **`reach NAME [EVTERM] [from RESULT]`** is `lfp` from a seed over an
  arbitrary term, and `EVTERM` admits `(seq (when q) EVENTS)`: the
  q-constrained forward closure `FwdU(p, q)` is expressible on the surface
  now, and if `q` is separable it saturates as an ordinary event set because
  the filtered events are still products. `(apply …)` is `EY`.
* **Parser and normal form**: `petri/expr/CtlFormula.h` (Pred, Deadlock,
  Not/And/Or, EX AX EF AF EG AG EU AU EW AW), `CtlSimplify.h` (NNF, TAPAAL's
  collapsing rewrites, booleans over leaves merged into one predicate — i.e.
  one selector), the MCC XML handler with CTL, the `(ctl NAME f)`
  s-expression grammar, `PropertyKind::CTL`. `hsc-pn` already loads
  properties and runs a surface session per question.
* **Oracles.** `its-ctl` (the reference), the MCC oracle vectors (CTLC/CTLF
  campaigns already run at 1954 instances each on the cluster, so the
  comparison set exists), PetriSpot's explicit checker for witnesses, and
  `.solved` files under PetriSpot `bench/models/*/CTL*` for small nets.
* **Not there**: any backward operator; `has_image`; a meet on terms or a
  gfp; a CTL walk; surface forms for the above.

## 4. The four core pieces, in HSC terms

### 4.1 `has_image` — the existential interpretation of a term

Semantics: `has_image(h, S)` is `0` iff `h(S) = 0`, and otherwise some
nonempty `W ⊆ h(S)`. It is a second reading of the same term algebra, which
is exactly how the calculus wants to be read ("a term is interpreted by
whichever algebra it is handed to"):

* `sum`: first summand with a nonzero answer; `compose(a, b)`:
  `has_image(a, has_image(b, S))`, and on `0` fall back to
  `has_image(a, b(S))` — the libDDD fallback, honest;
* `node(h, t)` on a diagram: walk the arcs, return the first rectangle
  `(has_image(h, P_i), has_image(t, S_i))` with both nonzero; `node(id, t)`
  descends without touching the head;
* `lfp h` and `saturate`: the closure contains its seed, so the answer is `S`
  itself when `S ≠ 0` — this is why `E[f U g]` at the top costs one atom
  evaluation in libITS;
* `expr` (case bracket): the first class with a nonzero residual;
* at a leaf: the theory's `has_image_local` (for `int_set`, apply until one
  element survives);
* the gfp form of §4.3 gets the fast SCC push-down.

Cost: one new recursion over `op_kind` beside `do_apply`, one theory
virtual, and a cache keyed like `ops_` (libDDD caches it). Surface:
`(exists NAME EVTERM SOURCE)` binding the witness subset, so `hsc-pn`'s
`nonempty` stops paying for the full selection. This piece alone is the
"stop on first response" discipline the user asks for, and it is also the
emptiness-check-of-a-construction idea: an LTL-style on-the-fly stop, kept
symbolic — the construction is the term, the check walks it lazily and
returns the first nonempty cell.

### 4.2 The inverse, relative to a context

The calculus exports pushforwards only (Definition 2.3; `int_set.hh`: "no
preimage and none is wanted"). For backward CTL we need `pred = next⁻¹`.
Two facts make this tractable in HSC, and one makes it delicate.

**Inversion is structural on terms and preserves locality.**
`node(h, t)⁻¹ = node(h⁻¹, t⁻¹)`, `(Σ hᵢ)⁻¹ = Σ hᵢ⁻¹`, `(a ∘ b)⁻¹ = b⁻¹ ∘ a⁻¹`,
and an inverted event touches exactly the positions of the original. So the
F/L/G partition of the backward system is the partition of the forward one
and `saturate()` over the inverted events *is* backward saturation; nothing
new in the schedule. (libDDD needs a `dynamic_cast` per hom class; ours
falls out of `op_kind`.)

**Leaf inverses of `int_set` local terms** (`guard then action`):

| forward | inverse | context needed |
|---|---|---|
| `keep(g)` | `keep(g)` | none |
| `shift(g, d)` (Petri arcs) | `shift(g[x ↦ x−d], −d)` | none for exactness; a **bound** to terminate under closure (H2: `m += w` inverting `m −= w` has no upper guard) |
| `assign(g, c)` | `keep({c})` then havoc over `g ∩ D` | the leaf domain `D`, to enumerate `g` when symbolic |
| `apply_if(g, e)` | the relation `{e(v) ↦ v : v ∈ D, g(v)}`, as a sum of `keep({u})`-then-havoc-set terms | `D` |
| `havoc(g, lo, hi)` | `keep([lo,hi))` then havoc over `g ∩ D` | `D` |

The context `D` is the declared `(leaf NAME LO HI)` domain when present
(exact: true predecessors, reachable or not), else the projection of `R` on
that position (over-approximate as a relation, like libDDD's product of
projections). `havoc_if` exists but takes an interval; a havoc over a set
code is the one new local term.

**Case brackets** (`when g do lhs := rhs` spanning a cut — DVE, not P/T
nets): a symbolic inverse exists when each assignment is invertible given
the unchanged reads (`x := x + y` ⇒ `x := x − y`), which is the GAL
`invertExpr` route; the general case needs the relation as data. Propose:
invert what is invertible, **refuse loudly** otherwise (`(unsupported)`),
and note that **every P/T transition is separable** (Cor. 6.3), so the whole
MCC CTL corpus needs only the first two table rows: a Petri net's inverse
is the net with arcs reversed.

**Exactness and the `∩ R` question.** With exact leaf inverses the
backward set may contain unreachable states, and that is *harmless* for the
answers: an unreachable state has no reachable predecessor (if `s ∈ R` and
`s → u` then `u ∈ R`), so every reachable state in `lfp((sel_f ∘ pred) + id)(Sat g)`
is a genuine `E[f U g]` state, and negation is `R ∖ X` anyway; the gfp
starts from `Sat f ⊆ R` and only shrinks. With over-approximate inverses
(projection contexts, non-injective assignments) libITS's protection is the
model: test `pred(R) ⊆ R` once, keep the exact events raw, wrap the
offenders as `meet(·, R)` — a data-constant filter which is a G term at the
root (a diagram read as a term is its sum of rectangle products, each
straddling the cut), i.e. exactly the intersection that breaks saturation,
confined to the events that need it. The unknown to *measure* is the other
cost: backward sets over `R`'s complement can be far less structured than
`R` (the classic reason backward analysis is slower), and the declared-bound
clamp is what keeps them finite at all.

### 4.3 Meet on terms, and the deflationary closure

The term algebra has no `∩`: `(f ∩ id)` is not a term (paper, future work
item 7). Two options: a general binary `op_kind::meet` with
`(a ∩ b)(S) = a(S) ∩ b(S)`, or the single derived form `gfp h` =
greatest fixpoint of `X ↦ X ∩ h(X)` below the argument, the dual of `lfp`.
The derived form is enough for CTL (`EG`, `EH`/`FwdG`, `hasSCCs`) and
matches how `lfp` was admitted (offered as the primitive so recognising an
accumulation never requires matching operands). libDDD has no saturation for
it either: the top-level loop is breadth-first, and only `has_image` gets
the per-level `F * id` push-down. Start the same way — naive gfp over the
memoised `apply`, plus the sufficient-witness push-down for `has_image` —
and record whether a canny schedule exists as the open question it is.

Two cheap rules from the reference worth keeping: `hasSCCs` once per model
(acyclic graph ⇒ `EG` is deadlock-terminated paths only, an lfp), and
deadlocks before any gfp (`dead_f ≠ ∅` answers an existential `EG f`
immediately).

### 4.4 Saturation under a constraint

Where libDDD needs the commutation rewrite of §2, HSC gets most of it from
its composition law. A constrained system is `Σ (sel_f ∘ hᵢ)` (backward:
`hᵢ = tᵢ⁻¹`). If `sel_f` is **separable** — a product of leaf filters, which
is every Fireability atom and every single-place Cardinality atom — then
`sel_f ∘ hᵢ` is a product whose support is `supp(sel_f) ∪ supp(hᵢ)`, with
the filter fused into the leaf terms it shares with `hᵢ` (`keep_if(g) ∘ shift`)
and standing alone as `keep_if` at the others. `saturate()` then partitions
these composed products by the ordinary F/L/G rule. The libDDD split
reappears as a *consequence*: an event disjoint from the filter's support
keeps its locality but drags the filter's leaves into its support — which
is precisely *not* what the rewrite does (it saturates such events
unconstrained and filters once). So the rule is still needed, in this form:

> for `lfp(Σ (sel ∘ hᵢ) + id)`, split `hᵢ` into those whose support is
> disjoint from `sel`'s (they commute) and the rest; rewrite to
> `lfp( id + lfp(Σ_{notC} sel ∘ hᵢ + id) + sel ∘ lfp(Σ_{doC} hᵢ + id) )`.

In HSC the commutation test is syntactic on the term (disjoint leaf
supports, or both selectors), like the saturation split. For a **crossing**
selector (a Cardinality sum over several places, a case bracket) the
composition `sel ∘ hᵢ` is a G term at the cut it straddles for every `hᵢ`
that touches those places; for the others the rule moves the filter out of
the loop. This is the piece to write down as an algorithm before coding —
one rewrite in `saturate()`'s vocabulary — and the one whose payoff the
campaign will measure.

## 5. The checker

A new package `include/hsc/ctl/` (algorithm.md first), above `surface`,
because it needs the session's objects: the event term, `R`, the init seed,
`apply_atom`. Structure mirrors `CTLChecker` minus VIS:

1. `CtlFormula` → NNF (`ctlNnf`) → existential form: `AX f = ¬EX¬f`,
   `A[f U g] = ¬(E[¬g U ¬f∧¬g] ∨ EG¬g)`, `A[f W g] = ¬E[¬g U ¬f∧¬g]`,
   `E[f W g] = E[f U g] ∨ EG f`, `EF/AF/AG` by the usual. Then the forward
   conversion of `Init ∧ φ` (VIS rules of §2), which pushes `Init` inward and
   turns the top of the tree into `EY`, `FwdU`, `FwdG` and `Cmp` nodes.
2. Per node: a **selector term** when the subformula is a state formula
   (the vendored simplifier already merges those into one predicate), else a
   **Sat set**; both memoised per node.
3. Evaluation, existential at the top (`need_exact = false` propagating
   through `or`, `EX`, `EU`, `EG`, `FwdU`, `FwdG`; the `Cmp` node is a
   `has_image`), exact below.
4. Answer: `FORMULA NAME TRUE|FALSE TECHNIQUES DECISION_DIAGRAMS SATURATION`
   plus, optionally, the witness subset via `get-witness`.

Surface forms, so a `.hsc` file can be its own CTL test and `hsc-pn` stays a
thin driver: `(pred NAME EVTERM SOURCE)` (one backward step),
`(gfp NAME EVTERM from RESULT)`, `(exists NAME EVTERM SOURCE)`, and
`(ctl NAME FORMULA)` reusing the vendored `(ctl …)` grammar, with
`(expect-ctl NAME TRUE|FALSE)` for self-checking files.

## 6. Order of work and distance

Milestones, each testable alone against `its-ctl` / the oracle:

| # | what | where | size (guess) |
|---|---|---|---|
| M0 | `ctl/algorithm.md`; the four term extensions specified in `core/algorithm.md` vocabulary (has_image, gfp, inverse-with-context, constrained-closure rewrite) | docs | — |
| M1 | `has_image`: core recursion + `int_set` local + cache; `(exists …)`; `hsc-pn` `nonempty` switches to it (a speed win on RC/RF today, measurable at once) | core, leaves, surface | ~250 LOC |
| M2 | `gfp` op kind, naive evaluation, fast-SCC push-down in `has_image`; `(gfp …)`; `hasSCCs` | core, surface | ~200 LOC |
| M3 | **Forward CTL checker** on M1+M2: parser → existential form → forward form → evaluation; `hsc-pn` answers CTLC/CTLF; differential vs `its-ctl` on `examples/mcc` and PetriSpot `bench/models` `.solved` | ctl/, tools | ~600 LOC |
| M4 | Inverse with context: `int_set` local inverses (havoc-over-set as the one new local term), structural term inversion, exactness test and per-event `∩ R` protection; `(pred …)`; backward `EX/EU/EG` for the nodes forward form leaves existential-below (the `EX/EU/EG` under a `Cmp`) | leaves, core, surface | ~400 LOC |
| M5 | Constrained-closure rewrite in `saturate()` (§4.4); measure against M3's plain form | core | ~150 LOC |
| M6 | Campaign: CTLC/CTLF at 600 s on the cluster beside the existing ITS-Tools sets; witness cross-check with PetriSpot's explicit checker | experiments | — |

M3 already answers real MCC formulas: with the forward conversion the whole
top of the tree is `next`-only, and only the subformulas the conversion
leaves as `EX/EU/EG` (those under a `Cmp` or a non-convertible negation)
require M4. On the campaign's formula shapes (PetriSpot `CTL_PLAN.md` §2:
3–5 quantifiers deep, E and A interleaved) most formulas will hit M4 nodes,
so M3 alone decides a fraction; the distance to "a checker on par with
`its-ctl` in coverage" is M1–M4, to "on par in speed" M5 plus the
measurement.

## 7. Risks and what to measure

* **Backward blow-up** without `∩ R` (§4.2): node counts of backward sets vs
  `R`, per model; the exactness test's verdict per transition.
* **The gfp is breadth-first** (§4.3): rounds to convergence on `EG` /
  `FwdG`; how often `hasSCCs` or the deadlock rule removes it.
* **Crossing selectors in a loop**: a Cardinality sum composed with every
  event is a root-level G term per round — the case where libITS prefers the
  data form `Sat_f ∩ pred(X)` over the hom form. Keep both and let the
  measurement choose, as the roadmap does for the other accelerators.
* **H2** becomes live: an inverted Petri arc is an unguarded upward shift;
  the declared bound (`hsc-pn`'s `--bound`) must clamp the inverse or the
  closure diverges. This is the first place the calculus needs a domain for
  correctness rather than for counting.
* **Semantics at deadlocks** (a deadlock ends its path: `EG f` holds at a
  dead `f`-state, `EX` is false there, `AX` true) — the reference and the
  oracle agree; the explicit checker was caught once on it. The `dead_f`
  term is how the symbolic engine gets it right; keep it in every `EG`.

## 8. Not a paper yet

The engineering value is direct (MCC CTL examinations with the same driver
we already run). The calculus content — inversion preserving the F/L/G
partition, constrained closure via the composition law, `has_image` as a
second interpretation of the term algebra, and whatever the gfp schedule
turns out to be — belongs in `hsc_core.md`'s future-work item 7 today and
becomes a section only if M5's measurement says the rewrite matters.
