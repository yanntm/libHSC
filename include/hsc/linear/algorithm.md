# Invariants to a diagram, and impossibility from it

## 1. The invariant set

A P-flow `f` of a net gives `Σ_p f_p · m(p) = K_f` on every reachable
marking, `K_f = Σ f_p · m0(p)`. A semiflow (`f ≥ 0`) with `K_f` also bounds
every place it covers: `m(p) ≤ ⌊K_f / f_p⌋`. So, from the flows:

* the **structural zeros** first: a place with no token initially and no
  producer among the transitions that can fire stays empty, a fixpoint over
  the net alone; its domain is `{0}`, the tightest bound there is, and its
  consumers cannot fire. On BugTracking-PT-q3m016 this alone is the whole
  answer (§3);
* a **domain** per covered place, `[0, B_p]` with `B_p` the least such
  bound; the uncovered places keep the domain the model declared (a cap,
  not a bound — §4);
* the **NUPN facts**, when the PNML carries a unit tree tagged safe: every
  place is bounded by 1 (so every place is covered), and each unit's local
  places hold one token in all — `Σ_{p ∈ u} m(p) ≤ 1`, an inequality the
  knapsack builds by keeping the residual sums below the constant instead
  of pruning them (`(at-most …)`). Cheap facts the flows may not carry
  (a unit is a mutual exclusion the state equation does not always see);
* the **full product** `F` of those domains: one arc per leaf value, the
  diagram of every marking in the box;
* the **invariant set** `S = F ∩ {m : every flow holds}`: each flow one
  crossing atom `(== (+ (* f_p p) …) K_f)`, applied as a selector to `F` —
  the case-bracket machinery of the calculus, nothing new.

On the places the facts bound, the projection of `S` contains the
projection of `R`; a place no fact bounds keeps a cap, and there `S` says
nothing: a question that reads such a place is not asked of `S` (a dead
marking is the exception — clipping capped places keeps it dead, so a
deadlock `S` has no marking for is refuted soundly).
`S ⊇ R` on the covered places (every reachable marking satisfies every flow and lies in the box),
and `S` is closed under firing wherever the box is a bound: a reachable
successor of a marking of `S` is in `S`. The size of `S` is the risk: one
semiflow is a counter along the spine (nodes about `K × places`), several
multiply, and the shape decides how badly — the same shape question as for
`R`, and the same instruments (`(stock S)`) read it.

## 2. Two tests of deadness

For a transition `t` with guard `en(t)` (a selector):

1. **Never enabled**: `en(t)(S) = ∅` ⇒ `t` is dead. One selector application.
2. **One step of induction**: `t` disabled initially, and
   `en(t)(S) ∩ next(S ∖ en(t)(S)) = ∅` ⇒ `t` is dead. Because a reachable
   enabling marking would have a reachable predecessor that does not enable
   `t` (the first enabling marking on the path from the initial one), that
   predecessor is in `S`, and its successor would be in the image.

The test is computed **backward, per slice**. A flow is an equality,
preserved by firing in both directions, so the predecessor of a marking of
`S` satisfies every flow; the converse events (`core/algorithm.md` §9),
inverted against `S` and restricted to leaf domains as wide as the box, are
exact converses of real firings. Then `en(t)(S) ∩ next(S ∖ en(t)(S)) = ∅`
reads `pre(en(t)(S)) ∩ (S ∖ en(t)(S)) = ∅`: no marking of `S` outside the
slice enters it. Two restrictions make it cheap where the forward image
was not (the image of `S` under all events, or under the live ones, did not
return in 385 s on BugTracking — `S` is the largest object in reach):
only the **live** events count (a transition proved dead never fires from a
reachable marking, and the first enabling marking of `t` is reached by a
live one), and only those **writing a leaf the guard of `t` reads** (a
firing that changes none of them leaves the guard's value as it was). The
test **iterates**: every kill shrinks the live set, and the passes repeat
while a transition falls. The untested transitions (no exact guard) stay
live: they may fire. Stopped, the verdicts taken stand — each rests on
tests that completed.

The same test with any set `X` in place of `en(t)(S)` refutes a
reachability target (`init ∩ X = ∅ ∧ pre(X) ∩ (S ∖ X) = ∅` ⇒ `X`
unreachable); `k` steps are `k`-induction; the fixpoint `pre*(X) ∩ S`
decides — the initial marking is in it iff `X` is reachable — finite on
the covered places and small when `X` is unreachable: backward analysis
with the garbage predecessors pruned by the invariants (`(backward …)`,
`hsc-pn --approx-back`). Soundness of an *unreachable* verdict by the
backward test asks every place to be exact in `S` (a capped predecessor
would be missed) — the abstraction by removed places gives that; a
*reachable* verdict (a layer meets the initial marking) is a real path of
the net the converses belong to, hence of the original net only when no
place was removed: the abstract net has more behaviour, and its paths need
not exist. The two verdicts are sound in complementary settings — the
capped set for paths, the abstract net for closures — and the full
corpus sweep found the two wrong verdicts that taking both on the
abstract net produces (`research_notes/unreach_report.md` §2.7).

Both tests are sound (they under-approximate the dead transitions) and
incomplete (a spurious marking of `S ∖ R` may enable `t`, or reach an
enabling one).

## 3. Reading the result

`(dead NAME SET [step] [ignore LEAF*])` prints, for the default system's
events, the ones dead by test 1, then those dead by test 2 (`step`), and a
summary; `hsc-pn --dead S [--dead-step]` does it from a net: flows within S
seconds, the structural zeros, the box, the equality diagrams (§5), the
tests, the names under `-v`. The oracle of the test is the QuasiLiveness
examination: a transition not quasi-live is dead.

Measured (the QLA oracle of `pnmcc-models-2026` as the reference):

| net | transitions | dead found | oracle dead | wrong | time | note |
|---|---|---|---|---|---|---|
| BugTracking-PT-q3m016 (Sloan) | 27 370 | 24 601, all by the structural zeros | 24 601 | 0 | 15 s | ITS-Tools: the same 24 601 structurally, then 61 s of SMT for 1098 more the oracle leaves unknown (2523 unknown in all). The forward one-step test (`--dead-step`) built `S` in 15 s and did not finish its image in 385 s, under all events or under the 2769 live ones |
| SupplyChain-PT-00005 | 43 | 0 | 0 | 0 | 0.03 s | 37 flows, 12 positive, 41 of 69 places covered |
| TwoPhaseLocking-PT-nC00100vN | 6 | 0 | 0 | 0 | 0.2 s | the invariant set equals the reachable set here |

Two lessons: the shape matters for the invariant set as it does for the
reachable set (SupplyChain's set: 2.2 s under the NUPN order, 0.03 s under
Sloan); and the tests must not be filtered by whole presets — a transition
with a never-marked input place is dead whatever its other inputs, capped
or not, so the guard atoms on capped places are *dropped* (a weaker guard,
sound for "never enabled") rather than the transition skipped; the one-step
test alone needs the exact guard.

## 5. Building the equalities

The case-bracket curry of `(== (+ …) K)` over a full box enumerates residual
classes level by level and does not return on a few dozen constraints
(SupplyChain's 37). The equality of a nonnegative flow is built directly
instead (`equality.hh`): a knapsack along the shape, one node per (sort,
position, remaining sum), the residuals above `K` pruned, the domains read
from the box; `(intersect S F E_1 …)` meets them, tightest first.
Milliseconds where the curry never finished. Mixed-sign flows are not
constraints this construction takes yet.

## 4. Uncovered places

A place no semiflow covers keeps its declared domain, a cap: `S` then
under-approximates on that place and the tests are not sound for the
transitions whose enabling reads it — they are reported as untested. The
honest way through is an ω-value in the leaf theory (a ceiling meaning
"at least this"), `research_notes/ideas.md` #2.
