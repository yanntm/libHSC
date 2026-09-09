# Invariants to a diagram, and impossibility from it

## 1. The invariant set

A P-flow `f` of a net gives `Σ_p f_p · m(p) = K_f` on every reachable
marking, `K_f = Σ f_p · m0(p)`. A semiflow (`f ≥ 0`) with `K_f` also bounds
every place it covers: `m(p) ≤ ⌊K_f / f_p⌋`. So, from the flows:

* a **domain** per covered place, `[0, B_p]` with `B_p` the least such
  bound; the uncovered places keep the domain the model declared (a cap,
  not a bound — §4);
* the **full product** `F` of those domains: one arc per leaf value, the
  diagram of every marking in the box;
* the **invariant set** `S = F ∩ {m : every flow holds}`: each flow one
  crossing atom `(== (+ (* f_p p) …) K_f)`, applied as a selector to `F` —
  the case-bracket machinery of the calculus, nothing new.

`S ⊇ R` (every reachable marking satisfies every flow and lies in the box),
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

The image `next(S ∖ en(t)(S))` is one step of every transition over a set the
size of `S`: the cost class of one saturation round on `S`. Computed once
(`I = next(S)` is enough — `next(S ∖ en(t))` ⊆ `next(S)`, and the test
`en(t)(S) ∩ I' = ∅` with `I' = next(S ∖ en(t))` is sharper; the cheap
variant tests `en(t)(S) ∩ next(S) = ∅` first, sound, and the sharper one
only for the transitions that survive), it serves every transition: this is
where a diagram beats one linear program per (transition, producer, place).

Both tests are sound (they under-approximate the dead transitions) and
incomplete (a spurious marking of `S ∖ R` may enable `t`, or reach an
enabling one).

## 3. Reading the result

`(dead NAME SET)` prints, for the default system's events, the ones dead by
test 1, then those dead by test 2, and the count that remain; `hsc-pn --dead`
does it from a net's flows and prints the dead transitions' names. The
first measure is BugTracking-PT-q3m016: 27 370 transitions, most of them
dead by the structural tools; how many the invariant set alone kills, at
what size and time, under which shape.

## 4. Uncovered places

A place no semiflow covers keeps its declared domain, a cap: `S` then
under-approximates on that place and the tests are not sound for the
transitions whose enabling reads it — they are reported as untested. The
honest way through is an ω-value in the leaf theory (a ceiling meaning
"at least this"), `research_notes/ideas.md` #2.
