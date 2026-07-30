# The loop, as implemented

Reference: the spec (`research_notes/cegar_spec.md`) fixes formats,
oracles and milestones; this file is the code-level strategy. Words
are vectors of letter indices; a leaf's language `L_i` is the set of
words its partial deterministic `delta` fires to completion —
nonempty, prefix-closed.

## Classifiers

A classifier over `Σ` is a finite-index right congruence presented by
its canonical table: `k` classes named `0..k-1` in shortlex order of
their least access words (`reps[0] = ε`), right action `act[c][a]`,
`live` mask with at most one dead class, absorbing. Its language is
the live-class words; membership is a fold over `act`.

`canonicalize` takes any complete live/dead table and returns the
canonical classifier of its language: (1) merge all dead classes into
one absorbing sink — sound for prefix-closed targets, where one
non-trace condemns its whole cone; (2) Moore partition refinement on
(live, act) down to the Nerode quotient; (3) BFS from `[ε]` in letter
order to name each class by its shortlex-least access word, sort,
renumber. Idempotent; language equality becomes byte equality of the
serialization. Every published hypothesis passes through it, so every
rung is a canonical object and interning works mid-flight.

## Teacher

Two oracles per leaf, both leaf-local:

* membership — fire the word;
* certification — BFS over pairs `(q, c)` of `lts × table` from
  `(0, [ε])`, following defined leaf transitions only. A reachable
  pair with `c` dead exhibits a trace the hypothesis rejects: the
  access word is returned as a *positive counterexample*. None
  reachable proves `L_i ⊆ L(H_i)`; the walk is the certificate.

## Learner (L\* + Rivest–Schapire)

Observation table `(S, E, T)`: `S` prefix-closed access words with
pairwise-distinct rows, `E` distinguishing suffixes (initially `{ε}`),
`T` filled by membership. Close: promote any `s·a` whose row matches
no `S` row. Publish: table DFA (classes = rows, accept = `T(s·ε)`),
dead-closed (rejecting rows become the sink — certification, not
construction, is the soundness judge), then canonicalized.

Counterexample handling: for a word `w` misclassified by the published
classifier, binary-search the split point `i` where
`member(rep(class(w[..i])) · w[i..])` flips; the suffix `w[i..]` is a
new experiment; re-close. Each counterexample strictly grows `|S|`
(asserted), and `|S|` never exceeds the leaf's state count + 1, which
bounds the whole loop's refinement budget by `Σ_i (|Q_i| + 1)` over
distinct leaves.

## The loop

Profile: one entry per *distinct* leaf (keyed by the `lts`
serialization — interned leaves share learner, classifier,
certification, and budget), initially chaos (1 class, certified free).

1. **Search**: BFS over `(m, c_1..c_n)` — monitor state and one class
   per leaf instance. An event steps the monitor and the supported
   classes; a dead successor class blocks it. No reachable bad
   monitor coordinate → the reached set is an inductive invariant:
   emit the certificate and stop (holds). Else reconstruct the event
   word `w` by predecessors.
2. **Replay by descent**: walk the shape; skip subtrees `w` does not
   support, project at cuts, fire at leaves. All executions complete →
   stop (violation; the executions are the evidence). Else every
   failing leaf `i` yields `u_i = π_i(w) ∈ L(H_i) \ L_i`, a certified
   negative counterexample.
3. **Refine until resolved**, per culprit: feed `u_i`; publish;
   certify, feeding back positives until certified; repeat until the
   published classifier classifies `u_i` dead. Go to 1.

A refuted witness may recur in a later round (hypotheses are not
monotone); each recurrence burns budget, so termination stands.

## Certificate

`⟨ per-distinct-leaf tables, Inv = final reached set ⟩` with
obligations: per leaf, the certification walk (`L_i ⊆ L(H_i)`);
globally, `Inv` contains the initial state, is closed under every
event (dead-blocked counts as closed), and avoids `Bad`. Checked by
`tools/hsc-certcheck.cc`, an independent translation unit sharing no
code with this package — the trusted core is the checker, not the
loop.

## Oracles wired into tests and sweeps

Verdict parity with the monolithic concrete-product walk; per-round
strict growth of `Σ index(H_i)` and re-certification; the budget cap;
pointer-equality of interned profiles; every emitted certificate
re-checked, and mutated certificates rejected.
