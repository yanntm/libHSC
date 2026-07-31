# Spec — CAC08 sample corpus (`examples/cac08/`)

Engineering handout. Goal: rebuild the benchmark family of Cobleigh,
Avrunin, Clarke, *Breaking up is hard to do* (TOSEM 17(2), 2008 —
`papers/Cobleigh_Avrunin_Clarke_2008_TOSEM.{pdf,txt}`, key [CAC08] in
`cegar_citations.md`) as `.hsc` models, so the CEGAR loop can be
measured on the exact family where learned assume-guarantee was shown
to lose to monolithic checking.

## 0. Fidelity contract (read first)

* This is a **reconstruction, not a reproduction**. The paper
  describes the subjects in prose (its §4, Table I); the original Ada
  and FSP artifacts are not in our holdings. Matching their state or
  memory counts is a **non-goal** (different substrate, different
  encodings). What we reproduce is the *family and the study design*:
  the five scalable systems, the 32 property–system subjects, sizes
  swept upward, properties known to hold.
* **No internet.** Everything derives from the TOSEM text plus the
  classical, well-known behavior of these textbook systems. Where the
  paper underdetermines a modeling choice, make the simplest choice
  and **record it** in `examples/cac08/README.md` under "Modeling
  decisions"; where a subject's semantics is genuinely ambiguous
  (expect this for Chiron), stop and ask Theory rather than invent.
* Every emitted model is an oracle-checked example in the repo sense:
  expected verdict stated, checked by the monolithic walker.
  All 32 subjects were verified **true** in CAC08 — expected verdict
  is `holds` everywhere; a `violation` is a modeling bug (or a real
  find — either way, report it, do not tune it away silently).

## 1. Layout

```
examples/cac08/
  README.md            source map + modeling decisions + provenance
  gen_cac08.py         one generator, `--system S --size k --out f.hsc`
  peterson_2.hsc ...   committed emitted samples, smallest size each
  expected.tsv         subject id, system, property, size, verdict
```

Campaign records go to `experiments/cegar/` per the existing pattern;
prose responses to this spec go in `cegar_report.md`. Generator is
deterministic (no seeds needed — these are fixed families, not random
models).

## 2. The five systems

All rendezvous-based client–server systems except Peterson (shared
variables). Rendezvous = a synchronized event between exactly two
leaves — squarely inside the separable fragment. Scaling axis from
the paper, per system:

| system | leaves | scaled by k = |
|---|---|---|
| Gas Station | operator, pump(s), k customers | customers |
| Smokers | supplier, table/lock, k smokers | smokers |
| Peterson | k tasks + shared variables (each variable = one leaf) | competing tasks |
| Relay | shared variable, k tasks | tasks touching the variable |
| Chiron single | dispatcher, ADT/registration state, k artists | artists |
| Chiron multiple | one dispatcher per event kind, ADT, k artists | artists |

Classical behaviors (all textbook; keep each leaf minimal):

* **Gas Station** (Helmbold–Luckham): customer prepays to operator,
  operator activates pump, customer pumps (start/stop), pump reports
  charge to operator, operator gives change to customer.
* **Smokers** (Patil): supplier puts two of three ingredient pieces
  on the table; the smoker holding the third assembles and smokes;
  CAC08's version has an explicit mutual-exclusion lock around table
  access (their property 32).
* **Peterson** (1981): the standard 2-process protocol with flag
  variables and turn variable; k > 2 via the generalized/filter form.
  Start with k = 2 only; generalizing is a Theory decision — ask
  before modeling k > 2.
* **Relay** (Siegel–Avrunin): k tasks read/write one shared variable;
  the variable alternates 0/1 per its property.
* **Chiron** (Keller et al. 1991, versions per Avrunin et al. 1999):
  artists register/unregister for event kinds with a dispatcher; the
  dispatcher receives events and notifies the registered artists in
  registration order. The registration list is real state (property 8
  bounds its size). Single vs multiple dispatcher versions. **This is
  the reconstruction-heavy one — last milestone, expect a Theory
  round-trip on its semantics.**

Shape: balanced binary tree over the leaves with replicated instances
as sibling subtrees sharing one currified code (interning must fire:
check byte-equality multiplicity in the campaign output). Record the
chosen bracket in the README; it is a knob, not a truth.

## 3. Properties → monitors

Table I of the paper (its p. 7:11) lists all properties in English;
transcribe each as a safety DFA over the root alphabet, numbered by
CAC08 subject id (1–32; Chiron ids n/n+9 pair single/multiple).
Notes:

* "repeatedly first X then Y" (subjects 20, 21, 31) is the safety
  reading: the alternation automaton, bad prefix = two X without Y
  between (or Y before any X). Say so in the README.
* Properties name specific instances (artist1, customer1, customer2,
  smoker1, smoker2). The monitor refers to those instances' event
  copies; instances stay one interned code. Properties touching only
  instance 1 and 2 are our locality probes: the cone question is
  whether instances 3..k refine at all.
* Chiron subject 9 mentions program termination: model an explicit
  `halt` event. Modeling decision, README.

## 4. Milestones

* **M1 — pilot.** Scaffolding + Gas Station (subjects 19–22) and
  Peterson (subject 23) at smallest size (k = 2). Emitted `.hsc`
  committed; `expected.tsv` rows; monolithic walker verdict `holds`
  on all five; `hsc-cegar run` parity + certificate checker pass.
  **Stop, report** (include: event counts, leaf sizes, any modeling
  decision made).
* **M2 — easy remainder.** Relay (24) and Smokers (25–32), same
  discipline. Then the size sweep on everything modeled so far:
  k ∈ {2, 3, 5, 10, 20, 50, 100, 200}, 15 s cap per run, timeouts
  are rows. TSV columns: subject, k, verdict, parity, wall,
  abstract-states, concrete-states (when mono fits), budget spent vs
  T3 pool, rung profile per entry, interning multiplicity,
  certificate size, checker time, and the **teacher-cost split**
  (time in leaf oracles: membership + certification, vs time in the
  step-1 walk, vs replay). If the loop lacks the teacher-cost split,
  add it first — it is the measurement this corpus exists for.
* **M3 — Chiron.** Both versions, subjects 1–18. Expect to stop at
  least once for Theory input on dispatcher/ADT semantics before
  coding.
* **M4 — record.** Campaign TSV committed under `experiments/cegar/`,
  README cross-links, response paragraph in `cegar_report.md`.

## 5. Expected outcomes (falsifiable — print next to the data)

* All subjects: `holds`, parity with mono, certificate checks, budget
  within the T3 pool. Any violation of these four is a stop-and-report.
* These systems are client–server and tightly coupled — CAC08 chose
  them, and they sit near the paper's §8 overhead pole. We do **not**
  expect to win wall time against the monolith at small k. The
  deliverables are: (i) budget adherence and its distribution, (ii)
  per-behavior refinement (one client entry, k instances — flat in
  k), (iii) leaf-sized (L) obligations where CAC08's premises were
  half-system-sized — the qualitative table §9 of the paper wants,
  (iv) the cone on instance-local properties (do instances 3..k stay
  chaotic? on which subjects?).
* Prediction to test, per system: server/dispatcher contracts grow
  with k (as the §6 worked example's server does); client/artist/
  smoker contracts are constant-size. Where a server contract stays
  small on an instance-local property, the abstract product should
  stay near the monitor while the concrete product grows — report
  the crossover k where mono times out and the loop still answers.

## 6. Discipline reminders

≤15 s per run, one invocation per case; logs under `tests/**/logs/`
or `experiments/cegar/`, never `/tmp`; commit per increment; no
internet; when the paper's prose underdetermines and the choice feels
load-bearing, ask Theory instead of choosing.
