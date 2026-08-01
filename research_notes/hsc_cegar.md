# Certified Component Abstraction on a Shape

*Companion to `hsc_core.md` (the calculus). Working draft. Citation
keys resolve in the References; holdings and verification notes in
`cegar_citations.md` — we cite only what we have read.*

---

## Abstract

Model checking a system of communicating components pays, in the
worst case, for the product of its parts. Abstraction refinement
promises to pay only for what the property needs — if three questions
find good answers: what to abstract, whom to blame for a spurious
counterexample, and how much refinement may cost before it defeats
its purpose. This paper shows that when the system is given on a
**shape** — the tree structure of the hierarchical set calculus,
whose leaves hold components and which interns every event at the
unique node it is local to — all three questions are answered by the
structure itself. Abstraction lives at the leaves, as certified
over-approximating classifiers; blame is a descent of the tree in
which failed executions name their own culprits; refinement spends
from a budget bounded by the *sum* of the components' sizes, never
their product; and the verdict "holds" is an exportable certificate,
laid out on the tree and checkable by a program that shares nothing
with the prover. A CEGAR loop with its heuristic steps replaced by
structure, and a proof that outlives the run.

---

## 1. Introduction

**The problem.** The cost of model checking a concurrent system is
dominated by state explosion: a system of `n` components, each of
size `m`, has up to `m^n` states, and the properties one actually
checks rarely need more than a sliver of that space. Two families of
answers dominate. Symbolic methods represent the product compactly
and walk it wholesale [BCM+92, CLS01, TPHK09, TM21]. Abstraction
methods replace components by smaller over-approximations and refine
on demand — counterexample-guided abstraction refinement, CEGAR
[CGJLV00, CGJLV03] — paying only for the precision the property
extracts. The promise of the second family has always been rationed
by its heuristic core: *which* abstraction to start from, *how* to
analyse a spurious counterexample into a refinement step, and — in
compositional variants — *where* to cut the system into parts. The
sharpest available evidence on the last question is negative: a
systematic study of learned assume-guarantee reasoning over all
two-way decompositions of its benchmark subjects found that the vast
majority of decompositions explored *more* states than checking the
monolith, and that a best decomposition generalizing to larger sizes
existed for only a fraction of subjects [CAC08].

**The observation.** The hierarchical set calculus does not present a
system as a flat parallel composition that a verifier must then cut.
It presents a *shape*: a tree whose leaves hold the components and
whose events — product-shaped, one letter per touched leaf — are
interned at the unique node spanning their support. The decomposition
is not chosen by the verifier; it is the model's own dependency
structure, and it gives every component, every event, and (it will
turn out) every abstraction failure an *address*.

**The thesis.** The shape is a sufficient guide to abstraction. Read
along the tree, each heuristic step of CEGAR becomes structure:

1. **What to abstract** — every leaf, uniformly, starting from the
   coarsest abstraction there is; the property drags exactly the
   leaves it needs away from triviality, and the cone of influence is
   not computed but *emerges* as the set of leaves whose abstraction
   ever moved (§3, §4).
2. **Whom to blame** — a spurious abstract witness is replayed by a
   descent of the shape that skips untouched subtrees wholesale and
   ends in one execution per touched leaf; the executions that fail
   *are* the culprits, each certifying its own counterexample (§4,
   Theorem T2). No simulation against the concrete product, no
   heuristics.
3. **What refinement may cost** — a budget linear in the sum of the
   leaves' sizes, spent per *behavior* rather than per instance:
   byte-equal leaves share one abstraction, one learner, one budget
   (§5, Theorem T3). This is the performance theorem: refinement can
   never do worse than reconstructing each distinct component's
   minimal contract, one class at a time, and it does so only where
   the property insists.
4. **What survives the run** — on "holds", a certificate of `n`
   leaf-local inclusion obligations plus one small induction, laid on
   the shape, checkable independently, in parallel, and — after a
   leaf is edited — incrementally (§6, Theorem T4).

Soundness itself is a *local invariant* rather than a global
argument: no abstraction is ever consulted unless certified against
its leaf by a leaf-sized inclusion check, and the discipline that
maintains this — nothing a learner knows is visible to the loop until
certified — closes a trap into which the natural implementation
falls (§3, Theorem T1 and Remark 3.7).

**Outline.** §2 fixes the setting — shapes, behaviors, properties —
and imports the classical material (Myhill–Nerode, MAT learning). §3
develops certified classifiers and the publication discipline. §4
gives the loop and the blame theorem, run on a worked example. §5
proves the budget and prices the loop honestly, poles included. §6
makes the proof an object. §7 positions the result against CEGAR,
learned assume-guarantee, and symmetry reduction; §8 concludes.

---

## 2. Context

Everything in this section is either the calculus's setting restated
for self-containment, or imported classical material; nothing is
original here. Readers of `hsc_core.md` may skim to the scope
paragraph.

### 2.1 Shapes, events, behaviors

**Definition 2.1 (shape, leaves).** A shape `V` is a binary tree;
write `leaves(ν)` for the leaves below node `ν`; internal nodes are
cuts. Each leaf `i` has a fixed finite alphabet `Σ_i`; leaf codes are
position-relative (currified), so two leaves with the same local code
are, as objects, one code. A leaf's behavior is the trace language
`L_i ⊆ Σ_i^*` of a finite deterministic LTS
`C_i = (Q_i, Σ_i, q_i^0, δ_i)`, `δ_i` partial: nonempty,
prefix-closed, executable.

**Definition 2.2 (events, locality).** A finite event set `E` is
given; each `e ∈ E` has support `supp(e) ⊆ leaves(V)` and a letter
`e_i ∈ Σ_i` per supported leaf — an event *is* a product of leaf
letters (the standing separability assumption). The **top** of `e` is
the least common ancestor of `supp(e)`; every event is interned at
its top, which is the event's locality.

**Definition 2.3 (restriction, projection).** For a node `ν` and an
event `e` with support inside `ν`'s subtree, `e|_ν` keeps the
supported letters inside `ν`; the visible alphabet of `ν` is
`A_ν = { e|_ν : supp(e) ∩ leaves(ν) ≠ ∅ }`, with `A_V = E` at the
root. The projection `π_ν : A_μ^* → A_ν^*` (for `ν` below `μ`)
restricts event by event and erases events without support in `ν`.
Projections compose: `π_ν ∘ π_μ = π_ν`.

**Definition 2.4 (behavior of a node).** By recursion on the shape:
`L_i` at leaf `i`; at a cut `ν` with children `l, r`,

```
L_ν  =  { v ∈ A_ν^*  :  π_l(v) ∈ L_l  and  π_r(v) ∈ L_r }.
```

The **system behavior** is `L_V` at the root.

**Proposition 2.5 (leaf form).** `L_ν = { v ∈ A_ν^* : π_i(v) ∈ L_i`
for every leaf `i ∈ leaves(ν) }`.

*Proof.* Induction on the tree, composing projections: the
conjunction over the two children's leaves is the conjunction over
`leaves(ν)`. ∎

**Proposition 2.6 (cut decomposition).** At every cut, `L_ν` is the
Arnold–Nivat synchronized product [AN80, Arn82] of its children's
behaviors — Nivat's S-synchronization, verbatim: events are vectors
of per-component letters, an absent component waits. Here: events
interned at `ν` synchronize both children through their restrictions;
events interned strictly inside one child pass to it unchanged and
*skip* the other, wholesale.

*Proof.* Restatement of Definitions 2.2–2.4: `top(e) = ν` iff `e` has
support on both sides of the cut. ∎

The proposition is trivial by design, and the design is the content:
the flat synchronized product is recovered *at every cut at once*,
which is what makes blame a descent (§4) rather than a global
analysis. Bracketing does not change `L_V` (Proposition 2.5 mentions
no cut); what the tree adds is an address for every event and every
failure.

**Definition 2.7 (property).** A safety property is a complete DFA
`A = (M, E, m_0, δ_A, Bad)` over the root alphabet — the bad-prefix
monitor of a safety language in the sense of [AS85]. A **violation**
is `w ∈ L_V` with `δ_A(m_0, w) ∈ Bad`; the property **holds** if none
exists.

**Remark 2.8 (execution is linear).** Membership at any node is not a
search: fire the word. Leaves are deterministic, so `π_i(v) ∈ L_i` is
one execution of cost `|π_i(v)|`. A witness is data; checking it is
running it.

### 2.2 Imports

**Fact 2.9 (Myhill–Nerode).** For `L ⊆ Σ^*`, let `u ~_L v` iff
`∀x. (ux ∈ L ⟺ vx ∈ L)`. This is a right congruence; `L` is regular
iff its index `N(L)` is finite; the quotient is the unique coarsest
right congruence saturating `L`. (Standard; see [PP04].)

**Fact 2.10 (prefix-closed targets).** If `L` is prefix-closed then
`u ∉ L` implies `uΣ^* ∩ L = ∅`: negative information is
extension-closed, and `~_L` has a single **dead** class, absorbing
under the right action.

**Remark 2.11 (the index is small).** If `L` is the trace language of
a deterministic device with `m` states, words reaching the same state
are `~_L`-equivalent, so `N(L) ≤ m + 1`.

**Fact 2.12 (MAT learning).** A minimally adequate teacher answers
membership queries and receives counterexamples (words the current
hypothesis misclassifies). A learner of the L\* family maintains a
finite-index right congruence presented by representatives and
experiments, and guarantees: (F1) representatives are pairwise
distinguished by concrete experiments, hence pairwise
`~_L`-inequivalent, so the class count never exceeds `N(L)`; (F2)
processing a counterexample strictly increases the class count; hence
(F3) at most `N(L) − 1` counterexamples are ever processed, from all
sources, over the learner's life. Hypotheses are **not** monotone
across refinements — a rebuilt congruence may re-admit a previously
rejected word — and nothing below pretends otherwise. (Angluin
[Ang87]; the Rivest–Schapire counterexample handling [RS93]; any
member of the family with F1–F2 serves.)

### 2.3 Scope

Standing assumptions, held throughout: events are separable
(Definition 2.2 — every event a product of per-leaf letters, checked
statically); properties are safety (Definition 2.7); leaves are
finite, deterministic, and **white boxes** — the system is being
model-checked, so its parts are executable. What learning buys here
is not access, the black-box concern of model learning [Vaa17], but
*choice of what to represent*: a leaf the property never probes stays
at the trivial abstraction forever. The abstract walk of §4 is
explicit; the shape is the model's, not the verifier's choice.

---

## 3. Certified classifiers and the publication discipline

An abstraction of a component must satisfy two masters: the loop,
which needs a small object it can compose and search, and soundness,
which demands that the object over-approximate the component's every
trace. The classical answer keeps automata as artifacts and argues
inclusion globally. We take a different route, and it is the first
place the structure pays: abstractions are *congruences presented
canonically*, their over-approximation is established by a
*leaf-local* check, and the loop is forbidden — by an invariant, not
by care — from ever consulting an object that has not passed it. No
automata as separate artifacts: a hypothesis is a congruence, and the
device that walks it is the structure the congruence already carries.

**Definition 3.1 (classifier).** A **classifier** over `Σ` is a
finite-index right congruence given by its **canonical class table**:
classes named by their shortlex-least representatives, in shortlex
order (so class 0 is `[ε]`), the right action `[u]·a = [ua]`
tabulated, at most one **dead** class, absorbing; all other classes
live. Its language `L(≈) = { u : [u] live }` is prefix-closed by
dead-absorption. Serialization of the table is the classifier's
identity: **equality is byte equality**.

**Remark 3.2 (dead-closure is free).** For a prefix-closed target
`L`, any finite-index over-approximation `L'` may be dead-closed
without losing over-approximation: every prefix of a member of `L` is
in `L`, hence in `L'`, so `L ⊆ interior(L')`. Dead-closure,
minimization, and shortlex renaming together are the
**canonicalization** map: any complete live/dead table goes to the
canonical classifier of its (dead-closed) language. Canonicalization
is idempotent, and language equality becomes byte equality — the
property interning rests on.

**Definition 3.3 (chaos).** The chaotic classifier `⊤_Σ` is the
one-class table: index 1, everything live, `L(⊤_Σ) = Σ^*`. It is the
canonical classifier *of* `Σ^*`: the bottom rung is not a degenerate
device but the exact object of the trivial language.

**Definition 3.4 (certification).** A classifier `H_i` is
**certified** for leaf `i` if `L_i ⊆ L(H_i)` has been established by
the leaf-local product walk of `C_i` with the class table: search for
a reachable pair `(q, d)` with `d` dead. Unreachable: the inclusion
holds and the walk is the certificate. Reachable along `u`: return
`u ∈ L_i \ L(H_i)`, a **positive counterexample**. Cost
`O(|Q_i| · index(H_i) · |Σ_i|)`, leaf-local. Chaos is certified for
free.

**Definition 3.5 (teacher contract).** A leaf theory owes the loop
two oracles and nothing else: **membership**, executable (run the
word, Remark 2.8); **certification**, decidable or loud (establish
the inclusion or return a positive counterexample). Finite LTS leaves
discharge both by walks. The contract is the entire interface between
loop and leaf class; every theorem below is proved against it.

**Definition 3.6 (publication).** Each leaf's learner maintains
internal structure (an observation table: access words, experiments,
membership rows) and separately **publishes** a classifier — the only
object the loop ever sees. The published classifier is `⊤` until the
learner processes its first counterexample; thereafter it is the
canonicalization of the internal table, dead-closed. The loop's
invariant is: *the published classifier is certified before any use*
(chaos initially, for free; re-certified inside every refinement,
Algorithm 4.1 step 3).

**Remark 3.7 (the trap the discipline closes).** A closed observation
table is *not* an over-approximation of the leaf: closing already
distinguishes letters by initial-state firability, so the table
rejects words the leaf can fire. Using it uncertified is unsound in
the dangerous direction — it blocks abstract transitions and can hide
a real violation behind a "holds". Publication-as-chaos is what makes
the loop invariant hold from the first round without a certification
pass the initial profile has not earned; every later publication sits
behind the certification of step 3. (A certificate built on an
uncertified table fails its (L) obligation, so the checker of §6
rejects it: the trusted core does not inherit the trap.)

**Lemma 3.8 (white-box teachers are local).** Under the contract,
each leaf supports a minimally adequate teacher whose every answer is
leaf-sized: membership is one execution; the counterexample channel
is fed by certification (positive) and by replay (negative, T2). No
oracle touches another leaf or any product. ∎

**Definition 3.9 (profile, rungs).** A **profile** assigns each leaf
a certified published classifier. Rungs, by index: chaos (index 1) up
to the **exact** rung, the canonical classifier of `L_i` itself
(index `N(L_i)`). Every published rung is canonical (Definition 3.6
canonicalizes on every rebuild), so two leaves whose current
abstractions coincide as languages carry byte-equal tables
*mid-flight*, not only at convergence. With currified leaf codes,
byte-equal leaves share one classifier, one learner, one
certification, one budget — and every refinement of one is a
refinement of all (Remark 6.4).

**Lemma 3.10 (profile soundness).** If `L_i ⊆ L(H_i)` for every
leaf, then `L_V ⊆ L_V[H]`, where `L_V[H]` is Definition 2.4 with
each `L_i` replaced by `L(H_i)`.

*Proof.* Pointwise in the leaf form of Proposition 2.5. ∎

---

## 4. The loop: blame is a descent

With certified profiles in hand, the loop itself is short to state.
Its interest lies in what it does *not* contain: no analysis of
spurious counterexamples — replay names culprits by executing
projections — and no refinement heuristic — the learner's target, the
leaf's canonical classifier, is fixed in advance. We state the
algorithm, prove soundness and localization, then run both on an
example small enough to check by hand.

**The running example.** Clients `Cl_1..Cl_k`, each `I —g→ C —r→ I`
over `{g, r}`; a server over `{g_1, r_1, …, g_k, r_k}` with states
`{F, B_1, …, B_k}`: `F —g_i→ B_i —r_i→ F`. Event `g_i` syncs the
server's `g_i` with client i's `g`; likewise `r_i`. All clients are
one currified code: one shared profile entry. The property: "no grant
while a grant is outstanding" — monitor
`quiet —g_i→ outstanding —r_i→ quiet`, any second `g` from
`outstanding` is bad. It holds.

**Algorithm 4.1.** Maintain a certified profile; initially
all-chaotic. Repeat:

1. **Search.** Walk the abstract product: states
   `(m, [u_1], …, [u_n]) ∈ M × Π_i classes(H_i)`, transitions by
   `δ_A` on the event and the right action of each supported leaf's
   table on its letter, an event blocked when any supported class
   goes dead. If no state with `M`-coordinate in `Bad` is reachable:
   emit the certificate of §6 and stop — the property **holds**.
2. **Replay by descent.** A bad abstract path yields `w ∈ L_V[H]`
   reaching `Bad`. Descend the shape: a subtree containing no support
   of `w` is skipped wholesale (Proposition 2.6); at a cut, recurse
   with the projected words; at a leaf, execute. All leaf executions
   complete: stop — `w` is a **violation**, validated by the
   executions themselves.
3. **Refine.** Otherwise the **culprits** are the leaves whose
   execution failed; each culprit `i` has
   `u_i = π_i(w) ∈ L(H_i) \ L_i`, a certified negative
   counterexample. For each culprit (all of them, or a chosen subset
   — a policy knob): *refine until resolved* — feed `u_i` to learner
   `i`, republish, re-certify (feeding back any positive
   counterexample), until the published classifier is certified
   **and** classifies `u_i` dead. Go to 1.

**Theorem T1 (soundness).** If step 1 finds no bad reachable state,
the property holds.

*Proof.* The profile is certified whenever used (Definition 3.6's
invariant: chaos initially, re-certification inside step 3). By
Lemma 3.10 any violation would lie in `L_V[H]` and reach `Bad`; its
class-tuple run then exists in the abstract product and reaches a bad
state. ∎

**Theorem T2 (localization).** For `w` produced by step 1: (i) if
every leaf execution completes, `w` is a real violation; (ii) every
leaf whose execution fails yields `u_i ∈ L(H_i) \ L_i`, a sound
negative counterexample, certified by the failed execution itself;
(iii) if `w` is spurious, at least one leaf fails; and the descent
reaches every culprit while skipping, without inspection, every
subtree `w` does not touch.

*Proof.* (i) Completion means `π_i(w) ∈ L_i` for all leaves, so
`w ∈ L_V` (Proposition 2.5), and `w` reaches `Bad` by construction.
(ii) `w ∈ L_V[H]` gives `π_i(w) ∈ L(H_i)`. (iii) `w ∉ L_V` means
some projection is not a trace; skipping is Definition 2.3. ∎

Blame is executions, organized by the tree: no simulation against a
concrete product, no cores, no heuristics. The cost of locating a
failure is proportional to what the witness touches, not to the
system.

**Remark 4.2 (culprits are witness-relative).** T2 names the culprits
*of the witness the search returned*; a different search order
returns a different witness with a different culprit set, and several
leaves may be culpable at once. No theorem depends on the choice:
T1–T3 hold for every search order and every culprit-selection policy.
The locality that is *guaranteed* is per-behavior — refinement lands
on at most the touched entries — while which entries those are is a
property of the search, not of the calculus.

**The example, round 1.** Profile all-chaotic: the abstract product
is the monitor alone. Search returns a shortest bad word; a shortlex
BFS returns `w = g_1·g_1`. Replay: the server rejects `g_1·g_1` (no
`g` from `B_1`) — culprit; client 1 rejects `g·g` (no `g` from `C`) —
also a culprit, and by interning its refinement is every client's. (A
search returning `g_1·g_2` instead would indict the server alone —
Remark 4.2 in action; either way the loop is oblivious.) Both entries
refine: negative counterexamples split `[ε]` from the after-grant
class; the certification exchange feeds back the traces the
dead-closure cut (e.g. `g·r` for the client), driving both entries to
their exact rungs — client index 3, server index `k+2`.

**The example, round 2.** Search over the refined profile reaches
`{(quiet, F, I..), (outstanding, B_i, .., C_i, ..) : i ≤ k}` — bad
unreachable. The loop stops: **holds**, with the certificate of §6.

**Remark 4.3 (both verdicts are self-evident).** "Holds" carries the
§6 certificate; "violation" carries `w` and its leaf executions,
re-runnable by anyone. Neither asks for trust in the learner, the
search order, or the refinement policy — which culprit first, whether
to batch, when to jump a leaf straight to exact: all satisfy T1–T3,
so choosing among them is measurement, not proof.

---

## 5. The budget: refinement is linear in the parts

Termination arguments for abstraction refinement are usually
convergence arguments: the abstraction domain is finite, so
refinement cannot go on forever. That bounds nothing in practice. The
structure here affords something stronger, and it is the paper's
performance claim: refinement draws from a *pool* — one unit per
Nerode class per *distinct* leaf — so its total cost is linear in the
sum of the components' sizes, however large their product, and
however unlucky the search. The learner's target being fixed in
advance (the leaf's canonical classifier, not a predicate set to be
discovered) is what makes this a budget rather than a hope.

**Theorem T3 (progress and termination).** Define the budget pool
`B = Σ_i (N(L_i) − 1) + n` over distinct leaves. Every execution of
step 3 consumes at least one unit: a processed counterexample
(strictly growing that learner's class count, F2 — at most
`N(L_i) − 1` per leaf, F1+F3) or the leaf's unique publication switch
(chaos to table, Definition 3.6 — at most one per leaf, and it
strictly raises the published index from 1). The inner *until
resolved* loop spends from the same pool. Hence step 3 executes at
most `B ≤ Σ_i |Q_i| + n` times (Remark 2.11), and Algorithm 4.1
terminates with a verdict.

*Proof.* A culprit's `u_i` is misclassified by the published
classifier (T2(ii)); if the published classifier is table-backed it
shares the internal table's dead-closed language, so the table
misclassifies some prefix of `u_i`, and processing it grows the class
count (F2); if the published classifier is chaos, either the table
also misclassifies a prefix (F2 applies) or exposing the table is
itself the resolution step — the once-per-leaf switch. Positive
counterexamples from certification are misclassified by definition
and burn F2 budget. Between refinements, search and replay are
finite. The loop halts in step 1 (T1) or step 2 (a violation
validated by execution). ∎

**Remark 5.1 (what is *not* claimed).** The published index sum need
not grow monotonically round over round — canonicalization minimizes,
and hypotheses are non-monotone (Fact 2.12) — and a refuted witness
may recur in a later round; each recurrence burns budget, so T3
stands without either property.

**Remark 5.2 (the bound, read).** `B` is linear in the model:
refinement can never do worse than reconstructing every leaf's
canonical classifier, one class at a time, and it does so only where
the property insists. The exponential lives where it always lived —
in step 1's walk, `|M| · Π_i index(H_i)` at worst — but over
*classifier* indices: 1 for every leaf still chaotic. The cone of
influence is not computed; it is the set of leaves whose rung ever
left chaos, read off the final profile.

**The example, priced.** On the client–server family the budget pool
is `k+5` over the two entries (server `≤ k+1` counterexamples, client
`≤ 2`, two publication switches), and round 1 spends most of it: the
property's enforcer *is* the server, whose exact contract has `k+2`
classes, so refinement reconstructs it wholesale. This is the honest
pole: the concrete product of this family is small — every client is
tightly coupled to the server — and a monolithic walk beats the loop
on wall time. The example is minimal to the point of parody: every
leaf is load-bearing for this property, so nothing stays chaotic and
the abstract product is not smaller than the concrete one. What it
shows cleanly is the mechanics — culprits named by executing three
short words, refinement driven to canonical contracts by the
certification exchange — and what the loop still yields at this pole,
which a monolithic walk does not, is the certificate.

**The other pole.** On a ring of `N` identical stations with a
mutual-exclusion monitor, all stations are one profile entry
(Definition 3.9), so refinement cost is a constant independent of `N`
— the budget is per behavior, and only the invariant grows with the
system. Between the poles: the loop wins when the property is local
— when most leaves stay chaotic and the abstract product stays near
the monitor's size — ties with overhead when enforcement is global,
and T3 caps the cost of finding out.

Cheap, in summary: replay (executions proportional to the witness's
support, parallel over leaves); certification (leaf-local products);
refinement (budgeted, T3); certificate checking (Remark 6.2);
canonicalization (leaf-scale sorting). Not cheap: step 1's walk — the
irreducible cost of composition, honestly priced over classifier
indices. One scope line rather than a promise: classifiers here range
over each leaf's **full** alphabet, and interior nodes carry no
contracts of their own; contracts at cuts — where hiding meets
abstraction and projection can blow up — are outside this paper.

---

## 6. The certificate: proof as an exportable object

A verdict that must be trusted is worth less than a verdict that can
be checked. The loop's "holds" is the latter: everything the run
established is contained in a small object — one classifier per
distinct leaf, one invariant set — whose obligations a checker can
discharge with two kinds of walk and a closure scan, sharing nothing
with the prover but the input parser. The shape appears one last
time: the obligations attach to the tree's leaves, so the proof of a
large system is `n` small lemmas and one induction, checkable in
parallel, and re-checkable leaf-locally after an edit.

**Definition 6.1 (certificate).** `K = ⟨ (H_i)_i, Inv ⟩`: one
classifier per distinct leaf (instances alias their representative),
and a finite set `Inv` of abstract states of the step-1 product.
Obligations:

- **(L_i)**, one per distinct leaf, attached to the leaf:
  `L_i ⊆ L(H_i)`, checked by the walk of Definition 3.4;
- **(G1)** the initial abstract state is in `Inv`; **(G2)** `Inv` is
  closed under every event (an event blocked by a dead successor
  class counts as closed); **(G3)** no state of `Inv` is bad.

**Theorem T4 (certificates are sound, independently of provenance).**
If all obligations hold, the property holds.

*Proof.* (G1)–(G3): `Inv` is an inductive invariant of the abstract
product avoiding `Bad`, so no abstract violation exists; (L_i) for
all leaves with Lemma 3.10: `L_V ⊆ L_V[H]`; conclude as in T1. No
step refers to how `K` was produced. ∎

**Remark 6.2 (the checker is the trusted core).** Two kinds of walk
and a closure scan; no learner, no search heuristics, a few dozen
lines — implementable as a self-contained unit that shares only the
input parser with the loop, none of its logic, so a bug in the loop
cannot certify itself. A verification of a large model certifies as
`n` independent leaf lemmas plus one small induction — the
assume-guarantee shape, as data, laid out on the tree.

**Remark 6.3 (regression).** Edit leaf `i`; re-check (L_i) alone —
leaf-sized. Pass: the entire proof stands unread. Fail: the walk
returns the offending trace, which is a resumption point for the
loop. Contracts, operationally.

**Remark 6.4 (interning is symmetry, mid-flight).**
Canonical-at-every-rung (Definition 3.9) means byte-equal leaves
share one classifier and one (L) obligation throughout the run, and a
witness spurious at one instance refines all `k` siblings in
lockstep. The certificate for a symmetric system carries one leaf
lemma per *behavior*, not per instance — and the refinement budget is
per behavior too. Classical symmetry reduction [ID96, CEFJ96, ES96]
quotients the concrete state space by an automorphism group supplied
or discovered up front; here no group is computed — byte equality of
canonical tables *is* the symmetry detection, it applies mid-flight
to abstractions, and what it deduplicates is contracts, learning, and
budget, not the product walk.

**The example, certified.** For the client–server family at any `k`:
one (L) obligation for the server, **one for all `k` clients**, and
`Inv` with `k+1` states — two lemmas and a linear invariant, within
the `k+5` budget of §5.

---

## 7. Related work

**CEGAR.** The loop is a CEGAR loop [CGJLV00, CGJLV03] with its two
heuristic steps replaced by structure. The refiner is not predicate
discovery [GS97]: the learner's target — the leaf's canonical
classifier — is fixed in advance, which is what makes T3 a budget
theorem rather than a convergence hope. Spurious-witness analysis is
not a simulation against the concrete system: it is the descent of
T2, which names culprits by executing projections. What survives is
lazy abstraction's economy [HJMS02] — precision varying across the
model, just enough to verify the property — obtained here per leaf,
as the rung profile, rather than per control location as a predicate
set.

**Learned assume-guarantee.** The nearest line by ingredients:
assumptions for compositional verification learned by L\* [CGP03],
the journal treatment with symmetric rules and alphabet refinement
[PGB+08]; extensions made the assumption symbolic [AMN05], minimal
[CFC+09], and ω-regular [FCC+08]. None moved the teacher: throughout
the line, the assumption is one interface DFA at a *two-way* split,
and its oracles are answered by the split's sides — membership by
simulating a trace on one side, candidacy by model-checking each side
— so every oracle call is half-system-sized, and the split itself is
a choice the verifier makes blind. The systematic evaluation of that
line is the negative result of [CAC08]: over *all* two-way
decompositions of its subjects, under two verifiers, the vast
majority of decompositions explored more states than the monolith;
at the smallest sizes about half the subjects had no decomposition
beating it; the best decomposition generalized to larger sizes on
only 8 of 32 subjects under one verifier and 0 of 30 under the other.
The present design reads that result as being about *where the
teacher and the obligations live*, not about learning: here there are
`n` leaf contracts rather than one interface assumption, every oracle
is leaf-local (Lemma 3.8), the abstractions are canonical at every
rung and interned across instances, the decomposition is the model's
own shape rather than a verifier's guess, and the run exports a
certificate. [CAC08]'s subjects are reusable as a corpus for this
comparison; that is an evaluation matter, outside this paper's scope.

**Symmetry reduction.** Positioned in Remark 6.4: classical methods
[ID96, CEFJ96, ES96] quotient the concrete space by an up-front
automorphism group; interning deduplicates *abstractions* by byte
equality of canonical tables, mid-flight, with no group computation —
a different object is being quotiented, at a different time.

**Symbolic methods.** The house lineage [BCM+92, CLS01, TPHK09, TM21]
attacks the same explosion by representation rather than abstraction;
on regular structure, saturation walks state counts no abstract
product need ever reach. The two are not rivals on the same axis:
step 1 of Algorithm 4.1 is a reachability walk over a product of
small tables, and nothing above assumed it explicit — while what the
loop offers that a symbolic walk does not is the per-leaf contract
structure and the certificate. Model learning in the black-box sense
[Vaa17] solves a different problem — access — noted in §2.3.

---

## 8. Conclusion

Three old ideas — CEGAR's refinement discipline [CGJLV03],
assume-guarantee's local obligations in the learned line of [CGP03],
MAT learning's class-splitting [Ang87] — arranged on a shape so that
each supplies what the others lacked: the composition names the
culprits, so CEGAR loses its heuristic step; the learner is the
refiner, so refinement gains a canonical target and a budget; and
certification is leaf-local, so the equivalence oracle loses its
cost. In the separable fragment, blame is a descent, abstractions are
canonical objects at every rung, the leaf interface is two oracles —
membership and certification — and the verdict either re-runs
(violation) or re-checks (holds) without trust in the run that
produced it. What the loop knows, it proves; what it spends, it
counts; what it claims, a smaller program checks.

The loop is implemented in this repository as commands of the `hsc`
main, certificate and checker included; measurement against the
symbolic engines on the shared corpora is the open empirical
question, and its record is not part of this paper.

---

## References

Holdings: `~/git/Library` (see its `INDEX.md`) and the read-only
`~/git/BuchiToLTL/papers/` archive; per-entry verification notes in
`cegar_citations.md`. Preprint/report copies are cited without a
venue rather than with one recalled from memory.

- **[AMN05]** R. Alur, P. Madhusudan, W. Nam. *Symbolic compositional
  verification by learning assumptions.* CAV 2005, LNCS 3576,
  pp. 548–562.
- **[AN80]** A. Arnold, M. Nivat. *Controlling behaviours of systems:
  some basic concepts and some applications.* 1980.
- **[Ang87]** D. Angluin. *Learning regular sets from queries and
  counterexamples.* Information and Computation 75:87–106, 1987.
- **[Arn82]** A. Arnold. *Synchronized behaviours of processes and
  rational relations.* Acta Informatica 17:21–29, 1982.
- **[AS85]** B. Alpern, F. B. Schneider. *Defining liveness.*
  Information Processing Letters 21(4):181–185, 1985.
- **[BCM+92]** J. R. Burch, E. M. Clarke, K. L. McMillan, D. L. Dill,
  L. J. Hwang. *Symbolic model checking: 10^20 states and beyond.*
  Information and Computation, 1992.
- **[CAC08]** J. M. Cobleigh, G. S. Avrunin, L. A. Clarke. *Breaking
  up is hard to do: an evaluation of automated assume-guarantee
  reasoning.* ACM TOSEM 17(2), Article 7, 2008.
- **[CEFJ96]** E. M. Clarke, R. Enders, T. Filkorn, S. Jha.
  *Exploiting symmetry in temporal logic model checking.* Formal
  Methods in System Design 9:77–104, 1996.
- **[CFC+09]** Y.-F. Chen, A. Farzan, E. M. Clarke, Y.-K. Tsay,
  B.-Y. Wang. *Learning minimal separating DFA's for compositional
  verification.* TACAS 2009.
- **[CGJLV00]** E. Clarke, O. Grumberg, S. Jha, Y. Lu, H. Veith.
  *Counterexample-guided abstraction refinement.* CAV 2000, LNCS 1855.
- **[CGJLV03]** E. Clarke, O. Grumberg, S. Jha, Y. Lu, H. Veith.
  *Counterexample-guided abstraction refinement for symbolic model
  checking.* Journal of the ACM 50(5):752–794, 2003.
- **[CGP03]** J. M. Cobleigh, D. Giannakopoulou, C. S. Păsăreanu.
  *Learning assumptions for compositional verification.* TACAS 2003,
  LNCS 2619, pp. 331–346.
- **[CLS01]** G. Ciardo, G. Lüttgen, R. Siminiceanu. *Saturation: an
  efficient iteration strategy for symbolic state-space generation.*
  TACAS 2001.
- **[ES96]** E. A. Emerson, A. P. Sistla. *Symmetry and model
  checking.* Formal Methods in System Design 9, 1996.
- **[FCC+08]** A. Farzan, Y.-F. Chen, E. M. Clarke, Y.-K. Tsay,
  B.-Y. Wang. *Extending automated compositional verification to the
  full class of omega-regular languages.* TACAS 2008.
- **[GS97]** S. Graf, H. Saïdi. *Construction of abstract state
  graphs with PVS.* CAV 1997.
- **[HJMS02]** T. A. Henzinger, R. Jhala, R. Majumdar, G. Sutre.
  *Lazy abstraction.* POPL 2002.
- **[ID96]** C. N. Ip, D. L. Dill. *Better verification through
  symmetry.* Formal Methods in System Design 9:41–75, 1996.
- **[PGB+08]** C. S. Păsăreanu, D. Giannakopoulou, M. G. Bobaru,
  J. M. Cobleigh, H. Barringer. *Learning to divide and conquer:
  applying the L\* algorithm to automate assume-guarantee reasoning.*
  Formal Methods in System Design 32:175–205, 2008.
- **[PP04]** D. Perrin, J.-É. Pin. *Infinite Words: Automata,
  Semigroups, Logic and Games.* Elsevier Academic Press, 2004.
- **[RS93]** R. L. Rivest, R. E. Schapire. *Inference of finite
  automata using homing sequences.* Information and Computation,
  1993.
- **[TM21]** Y. Thierry-Mieg. *Symbolic and structural
  model-checking.* Fundamenta Informaticae 183(3–4):319–343, 2021.
- **[TPHK09]** Y. Thierry-Mieg, D. Poitrenaud, A. Hamez, F. Kordon.
  *Hierarchical set decision diagrams and regular models.* TACAS
  2009.
- **[Vaa17]** F. Vaandrager. *Model learning.* Communications of the
  ACM, 2017.
