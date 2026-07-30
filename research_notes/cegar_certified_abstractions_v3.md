# Certified Component Abstraction on a Shape

*Working draft 3. Standalone: everything used is stated; what is
imported is imported in §2 and nowhere else. A v1 implementation of
the finite instance exists (spec: `cegar_spec.md`; report:
`cegar_report.md`); where the text states a measured fact it names
that record.*

---

## 0. Stance and scope

The object of study is a verification loop over a system given as a
**shape**: a tree whose leaves hold components and whose events are
product-shaped, each interned at the unique node it is local to. Each
leaf carries, at any moment, a **certified over-approximation** of its
behavior — a finite classifier proved, by a leaf-local check, to
accept every trace the leaf has. The loop model-checks the property
against the classifiers, replays abstract witnesses by a descent of
the shape in which membership executions name their own culprit,
refines the culprit with a MAT-style learner, and terminates with
either a concretely validated violation or a compositional certificate
laid out on the shape, checkable without the loop.

Four claims, each a theorem below:

1. **Soundness is a local invariant, not a global argument** (T1):
   every abstraction in play is certified against its leaf by a
   leaf-sized inclusion check; the global claim composes one-line
   lemmas. The invariant has teeth: it is a *publication* discipline —
   nothing a learner knows is visible to the loop until certified
   (§3.6), and the one natural way to get this wrong is unsound
   (Remark 3.7, found by the implementation's independent checker).
2. **Spurious witnesses name their culprits by construction** (T2):
   replay is a descent of the shape ending in one execution per
   touched leaf; no abstract-counterexample simulation, no heuristics.
   Culprit *sets* depend on which witness the search returns; the
   verdict and the certificate do not (§6).
3. **Progress is budgeted, and the budget is linear** (T3): every
   refinement round spends from a pool bounded by the sum of the
   leaves' Nerode indices — for deterministic leaves, by the sum of
   their state counts — never by their product.
4. **The proof is an exportable object** (T5): n local inclusion
   obligations plus one small induction, checkable independently, in
   parallel, and — after a leaf is edited — incrementally.

Assumed, deliberately, with the relaxation that would remove each
named in §10: fixed alphabets in the separable fragment (every event
is a product of per-leaf letters, checked statically); safety
properties (bad-prefix monitors; the ω lift is a swap of the
classifier notion, not of the loop); explicit walks; the shape given
(its choice is a question about the model's dependency structure —
but locality is load-bearing here: "culprit" has an address because
the shape interns every event somewhere). Leaves are white boxes: the
system is being model-checked, so its parts are executable. What
learning buys is not access but *choice of what to represent*: a leaf
the property never probes stays at the free rung forever.

---

## 1. Setting: shapes, events, behaviors

**Definition 1.1 (shape, leaves).** A shape `V` is a binary tree;
write `leaves(ν)` for the leaves below node `ν`; internal nodes are
cuts. Each leaf `i` has a fixed finite alphabet `Σ_i`; leaf codes are
position-relative (currified), so two leaves with the same local code
are, as objects, one code. For §§1–5 a leaf's behavior is the trace
language `L_i ⊆ Σ_i^*` of a finite deterministic LTS
`C_i = (Q_i, Σ_i, q_i^0, δ_i)`, `δ_i` partial: nonempty,
prefix-closed, executable. (§7 widens the leaf class; nothing before
it depends on finiteness except where said.)

**Definition 1.2 (events, locality).** A finite event set `E` is
given; each `e ∈ E` has support `supp(e) ⊆ leaves(V)` and a letter
`e_i ∈ Σ_i` per supported leaf — an event *is* a product of leaf
letters (the standing separability assumption). The **top** of `e` is
the least common ancestor of `supp(e)`; every event is interned at
its top, which is the event's locality.

**Definition 1.3 (restriction, projection).** For a node `ν` and an
event `e` with support inside `ν`'s subtree, `e|_ν` keeps the
supported letters inside `ν`; the visible alphabet of `ν` is
`A_ν = { e|_ν : supp(e) ∩ leaves(ν) ≠ ∅ }`, with `A_V = E` at the
root. The projection `π_ν : A_μ^* → A_ν^*` (for `ν` below `μ`)
restricts event by event and erases events without support in `ν`.
Projections compose: `π_ν ∘ π_μ = π_ν`.

**Definition 1.4 (behavior of a node).** By recursion on the shape:
`L_i` at leaf `i`; at a cut `ν` with children `l, r`,

```
L_ν  =  { v ∈ A_ν^*  :  π_l(v) ∈ L_l  and  π_r(v) ∈ L_r }.
```

The **system behavior** is `L_V` at the root.

**Proposition 1.5 (leaf form).** `L_ν = { v ∈ A_ν^* : π_i(v) ∈ L_i`
for every leaf `i ∈ leaves(ν) }`.

*Proof.* Induction on the tree, composing projections: the
conjunction over the two children's leaves is the conjunction over
`leaves(ν)`. ∎

**Proposition 1.6 (cut decomposition).** At every cut, `L_ν` is the
Arnold–Nivat synchronized product of its children's behaviors: events
interned at `ν` synchronize both children through their restrictions;
events interned strictly inside one child pass to it unchanged and
*skip* the other, wholesale.

*Proof.* Restatement of Definitions 1.2–1.4: `top(e) = ν` iff `e` has
support on both sides of the cut. ∎

The proposition is trivial by design, and the design is the content:
the flat synchronized product is recovered *at every cut at once*,
which is what makes blame a descent (§4) rather than a global
analysis. Bracketing does not change `L_V` (Proposition 1.5 mentions
no cut); what the tree adds is an address for every event and every
failure.

**Definition 1.7 (property).** A safety property is a complete DFA
`A = (M, E, m_0, δ_A, Bad)` over the root alphabet. A **violation**
is `w ∈ L_V` with `δ_A(m_0, w) ∈ Bad`; the property **holds** if none
exists.

**Remark 1.8 (execution is linear).** Membership at any node is not a
search: fire the word. Leaves are deterministic, so `π_i(v) ∈ L_i` is
one execution of cost `|π_i(v)|`. A witness is data; checking it is
running it.

---

## 2. Imports

Everything taken from elsewhere, stated as used.

**Fact 2.1 (Myhill–Nerode).** For `L ⊆ Σ^*`, let `u ~_L v` iff
`∀x. (ux ∈ L ⟺ vx ∈ L)`. This is a right congruence; `L` is regular
iff its index `N(L)` is finite; the quotient is the unique coarsest
right congruence saturating `L`.

**Fact 2.2 (prefix-closed targets).** If `L` is prefix-closed then
`u ∉ L` implies `uΣ^* ∩ L = ∅`: negative information is
extension-closed, and `~_L` has a single **dead** class, absorbing
under the right action.

**Remark 2.3 (the index is small).** If `L` is the trace language of
a deterministic device with `m` states, words reaching the same state
are `~_L`-equivalent, so `N(L) ≤ m + 1`.

**Fact 2.4 (MAT learning).** A minimally adequate teacher answers
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
rejected word — and nothing below pretends otherwise. (Angluin; the
Rivest–Schapire counterexample handling; any member of the family
with F1–F2 serves.)

**Provenance 2.5 (the calculus).** The setting of §1 is the separable
fragment of the hierarchical shape calculus: events are product
terms, locality is interning at the top, skipping is the free
transmission of `id`, and currified leaf codes are position-relative.
The query/case bracket — which compiles a non-separable criterion
into finitely many `(event, residual)` pairs and thereby *discovers*
an exchange alphabet — is exactly what the separability assumption
excludes; it is the announced next relaxation (§10). This note needs
none of the calculus's theorems, only its setting.

**Deferral 2.6 (the ω target).** For liveness, the classifier of §3
is replaced by the syntactic invariant of an ω-language — the finite
algebra classifying lassos, with its own canonicity and
lasso-membership evaluation, developed in the invariant line of work.
The loop consumes classifiers through three verbs only — membership,
right action, certification — so the lift is a swap of the §3 notion,
not a rewrite (§10). This note stays with safety so that every proof
it claims, it contains.

---

## 3. Classifiers, certification, and the publication discipline

No automata as separate artifacts. A hypothesis is a congruence,
presented canonically; the device that walks it is the structure the
congruence already carries.

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
word, Remark 1.8); **certification**, decidable or loud (establish
the inclusion or return a positive counterexample; where the check is
only semi-decidable, divergence must be visible, never a clamp,
never a wrong answer). Finite LTS leaves discharge both by walks; §7
exhibits a second instance. The contract is the entire interface
between loop and leaf class; every theorem below is proved against
it.

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
table is *not* an over-approximation of the leaf. Closing already
distinguishes letters by initial-state firability, so the table
rejects words the leaf can fire — using it uncertified is unsound in
the dangerous direction: it can block abstract transitions and hide
real violations behind a "holds". This is not hypothetical: the v1
implementation initially published the closed table, produced false
"holds" verdicts on 5 of 200 random models, and the *independent
certificate checker* rejected every certificate built on an
uncertified table (obligation (L) of §5) — the trusted-core
architecture catching the loop's own bug is claim 4 doing its job
(record: `cegar_report.md`, finding F1). Publication-as-chaos is the
discipline that makes the loop invariant hold from the first round.

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
refinement of all (Remark 5.4).

**Lemma 3.10 (profile soundness).** If `L_i ⊆ L(H_i)` for every
leaf, then `L_V ⊆ L_V[H]`, where `L_V[H]` is Definition 1.4 with
each `L_i` replaced by `L(H_i)`.

*Proof.* Pointwise in the leaf form of Proposition 1.5. ∎

---

## 4. The loop

**Algorithm 4.1.** Maintain a certified profile; initially
all-chaotic. Repeat:

1. **Search.** Walk the abstract product: states
   `(m, [u_1], …, [u_n]) ∈ M × Π_i classes(H_i)`, transitions by
   `δ_A` on the event and the right action of each supported leaf's
   table on its letter, an event blocked when any supported class
   goes dead. If no state with `M`-coordinate in `Bad` is reachable:
   emit the certificate of §5 and stop — the property **holds**.
2. **Replay by descent.** A bad abstract path yields `w ∈ L_V[H]`
   reaching `Bad`. Descend the shape: a subtree containing no support
   of `w` is skipped wholesale (Proposition 1.6); at a cut, recurse
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
`w ∈ L_V` (Proposition 1.5), and `w` reaches `Bad` by construction.
(ii) `w ∈ L_V[H]` gives `π_i(w) ∈ L(H_i)`. (iii) `w ∉ L_V` means
some projection is not a trace; skipping is Definition 1.3. ∎

Blame is executions, organized by the tree: no simulation against a
concrete product, no cores, no heuristics. The cost of locating a
failure is proportional to what the witness touches, not to the
system.

**Remark 4.2 (culprits are witness-relative).** T2 names the
culprits *of the witness the search returned*; a different search
order returns a different witness with a different culprit set, and
several leaves may be culpable at once. (Measured: on the
client–server example of §6, a shortlex BFS returns a witness that
indicts both the server and a client, where a different witness
indicts the server alone.) No theorem depends on the choice: T1–T3
hold for every search order and every culprit-selection policy, so
the locality that is *guaranteed* is per-entry — refinement lands on
at most the touched behaviors — while which behaviors those are is a
measurable property of the search, not of the calculus.

**Theorem T3 (progress and termination — the finite instance).**
With finite LTS leaves, define the budget pool
`B = Σ_i (N(L_i) − 1) + n` over distinct leaves. Every execution of
step 3 consumes at least one unit: a processed counterexample
(strictly growing that learner's class count, F2 — at most
`N(L_i) − 1` per leaf, F1+F3) or the leaf's unique publication
switch (chaos to table, Definition 3.6 — at most one per leaf, and
it strictly raises the published index from 1). The inner *until
resolved* loop spends from the same pool. Hence step 3 executes at
most `B ≤ Σ_i |Q_i| + n` times (Remark 2.3), and Algorithm 4.1
terminates with a verdict.

*Proof.* A culprit's `u_i` is misclassified by the published
classifier (T2(ii)); if the published classifier is table-backed it
shares the internal table's dead-closed language, so the table
misclassifies some prefix of `u_i`, and processing it grows the
class count (F2); if the published classifier is chaos, either the
table also misclassifies a prefix (F2 applies) or exposing the table
is itself the resolution step — the once-per-leaf switch. Positive
counterexamples from certification are misclassified by definition
and burn F2 budget. Between refinements, search and replay are
finite. The loop halts in step 1 (T1) or step 2 (a violation
validated by execution). ∎

**Remark 4.3 (what is *not* claimed).** The published index sum need
not grow monotonically round over round as a matter of theorem —
canonicalization minimizes, and hypotheses are non-monotone
(Fact 2.4), so a refuted witness may even recur in a later round;
each recurrence burns budget, so T3 stands. (Empirically the
published index sum grew strictly in every round of every run of the
v1 campaign — the implementation asserts it and the assertion never
fired; a counterexample to that conjecture would be interesting, not
fatal.) Where there is no budget to burn, non-recurrence must be
manufactured — the negative store of §7, needed there and optional
here.

**Remark 4.4 (the bound, read).** `B` is linear in the model:
refinement can never do worse than reconstructing every leaf's
canonical classifier, one class at a time, and it does so only where
the property insists. The exponential lives where it always lived —
in step 1's walk, `|M| · Π_i index(H_i)` at worst — but over
*classifier* indices: 1 for every leaf still chaotic. The cone of
influence is not computed; it is the set of leaves whose rung ever
left chaos, read off the final profile.

**Remark 4.5 (both verdicts are self-evident).** "Holds" carries the
§5 certificate; "violation" carries `w` and its leaf executions,
re-runnable by anyone. Neither asks for trust in the learner, the
search order, or the refinement policy — which culprit first,
whether to batch, when to jump a leaf straight to exact: all satisfy
T1–T3, so choosing among them is measurement (§9), not proof.

---

## 5. Certificates on the shape

**Definition 5.1 (certificate).** `K = ⟨ (H_i)_i, Inv ⟩`: one
classifier per distinct leaf (instances alias their representative),
and a finite set `Inv` of abstract states of the step-1 product.
Obligations:

- **(L_i)**, one per distinct leaf, attached to the leaf:
  `L_i ⊆ L(H_i)`, checked by the walk of Definition 3.4;
- **(G1)** the initial abstract state is in `Inv`; **(G2)** `Inv` is
  closed under every event (an event blocked by a dead successor
  class counts as closed); **(G3)** no state of `Inv` is bad.

**Theorem T5 (certificates are sound, independently of
provenance).** If all obligations hold, the property holds.

*Proof.* (G1)–(G3): `Inv` is an inductive invariant of the abstract
product avoiding `Bad`, so no abstract violation exists; (L_i) for
all leaves with Lemma 3.10: `L_V ⊆ L_V[H]`; conclude as in T1. No
step refers to how `K` was produced. ∎

**Remark 5.2 (the checker is the trusted core).** Two kinds of walk
and a closure scan; no learner, no search heuristics. The v1 checker
is one self-contained program sharing no code with the loop; on the
first campaign it rejected every certificate the unsound
pre-discipline loop emitted and accepts all 56 the corrected loop
emits (record: `experiments/cegar/`). A verification of a large
model certifies as n independent leaf lemmas plus one small
induction — the assume-guarantee shape, as data, laid out on the
tree.

**Remark 5.3 (regression).** Edit leaf `i`; re-check (L_i) alone —
leaf-sized. Pass: the entire proof stands unread. Fail: the walk
returns the offending trace, which is a resumption point for the
loop. Contracts, operationally.

**Remark 5.4 (interning).** Canonical-at-every-rung (Definition 3.9)
means byte-equal leaves share one classifier and one (L) obligation
throughout the run, and a witness spurious at one instance refines
all `k` siblings in lockstep. The certificate for a symmetric system
carries one leaf lemma per *behavior*, not per instance — and the
refinement budget is per behavior too, which is where the measured
size-independence of §9 comes from.

---

## 6. A worked round: k clients and a server

Small enough to check by hand; aligned with what the implementation
actually does (record: `tests/cegar/`, `experiments/cegar/`).

**The model.** Clients `Cl_1..Cl_k`, each `I —g→ C —r→ I` over
`{g, r}`; a server over `{g_1, r_1, …, g_k, r_k}` with states
`{F, B_1, …, B_k}`: `F —g_i→ B_i —r_i→ F`. Event `g_i` syncs the
server's `g_i` with client i's `g`; likewise `r_i`. All clients are
one currified code: one shared entry. The property: "no grant while
a grant is outstanding" — monitor `quiet —g_i→ outstanding —r_i→
quiet`, any second `g` from `outstanding` is bad.

**Round 1.** Profile all-chaotic: the abstract product is the
monitor alone. Search returns a shortest bad word; a shortlex BFS
returns `w = g_1·g_1`. Replay: the server rejects `g_1·g_1` (no `g`
from `B_1`) — culprit; client 1 rejects `g·g` (no `g` from `C`) —
also a culprit, and by interning its refinement is every client's.
(A search returning `g_1·g_2` instead would indict the server alone
— Remark 4.2 in action; either way the loop is oblivious.) Both
entries refine: negative counterexamples split `[ε]` from the
after-grant class; the certification exchange feeds back the traces
the dead-closure cut (e.g. `g·r` for the client), driving both
entries to their exact rungs — client index 3, server index `k+2`.

**Round 2.** Search over the refined profile reaches
`{(quiet, F, I..), (outstanding, B_i, .., C_i, ..) : i ≤ k}` — bad
unreachable. Emit the certificate: one (L) obligation for the
server, **one for all k clients**, and `Inv` with `k+1` states
(measured at k = 2: 3 abstract states, 3 counterexamples spent, of a
budget of 7). Verdict parity, the witness economics, and the shared
client obligation are all measured facts of the v1 campaign (§9).

The example is minimal to the point of parody — every leaf is
load-bearing for this property, so the cone-of-influence phenomenon
is invisible here (nothing stays chaotic) and the abstract product
is not smaller than the concrete one. What it shows cleanly is the
mechanics: culprits named by executing three short words, refinement
driven by certification exchange to canonical contracts, and the
per-behavior certificate. The cone phenomenon needs a property that
ignores most leaves; §9 measures it where it exists.

---

## 7. Room left: well-structured leaves

Not the focus; recorded to fix the interface it exercises. The
teacher contract (Definition 3.5) never mentioned finiteness. Take a
leaf whose behavior is the trace language of a well-structured
transition system — an unbounded Petri-net-like structure under a
wqo with effective pred-basis. Then membership is still one
execution, and certification is reachability of the upward-closed
set `{(m, d) : d dead}` in `C_i ×` table — a **coverability**
question, decidable for WSTS by backward saturation (Abdulla et al.;
Finkel–Schnoebelen): the contract's "decidable or loud", discharged
on the decidable side.

T1, T2, T5 hold verbatim — their proofs used the contract, not the
walks. T3 fails, and must: `N(L_i)` is infinite already for one
unbounded counter.

**Proposition 7.1 (sound semi-algorithm, complete for violations).**
Augment the loop with (a) a **persistent negative store** per leaf —
every certified non-trace `d` ever produced by replay; the effective
abstraction is the classifier meet of the published table with the
canonical classifier of `Σ_i^* ∖ ⋃ d·Σ_i^*` (finite: a prefix-tree
quotient; over-approximation is preserved by Fact 2.2) — and (b)
**length-fair search** in step 1. Then the loop never reports a
false verdict, and every existing violation is eventually found.

*Proof.* Soundness: unchanged (certification is re-established on
every effective abstraction). Completeness: let `w` be a shortest
violation, `|w| = ℓ`. The store makes refutation permanent, so each
of the finitely many spurious words of length `≤ ℓ` is refuted at
most once; a real witness is never excluded (its projections are
traces; the store holds only certified non-traces). Length-fair
search reaches `w` after finitely many rounds; replay validates
it. ∎

The rung ladder specializes pleasantly: for a single unbounded
counter, certified classifiers distinguishing values up to `j` with
a chaotic tail are exactly the classical counter abstractions —
rediscovered as rungs, refined on demand, certified by coverability.
Making this a focus is a different note.

---

## 8. Cost shape, honestly

Cheap, and measured cheap: replay (executions proportional to the
witness's support, parallel over leaves); certification (leaf-local
products); refinement (budgeted, T3); certificate checking
(milliseconds on every v1 instance); canonicalization (leaf-scale
sorting).

Not cheap: step 1's walk, `|M| · Π_i index(H_i)` at worst — the
irreducible cost of composition, honestly priced: over classifier
indices, 1 wherever the property has not probed. Two measured poles
frame the promise (record: `experiments/cegar/tc_scaling.tsv`). On a
token ring of N interned-identical stations, refinement cost is flat
— 4 counterexamples whether N is 2 or 64 — because the budget is per
behavior (with interning ablated it grows as 2N, the verdict
unmoved). On the client–server family the property's enforcer *is*
the system: the server's exact contract has k+2 classes, refinement
reconstructs it wholesale, and the monolithic walk — tiny, because
clients are tightly coupled — wins on wall time. The loop wins when
the property is local, ties with overhead when it is not, and T3
caps the cost of finding out. What the overhead case still yields,
and the monolithic baseline does not, is the certificate.

One exposure deferred rather than hidden: classifiers range over
each leaf's **full** alphabet, and interior nodes carry no contracts
of their own. True interface contracts — a classifier at a cut over
the cut's visible alphabet, internal events hidden — are where
hiding meets abstraction and projection can blow up; Definition 1.4
is stated at every node precisely so that this extension changes the
*placement* of hypotheses and not one definition (§10).

---

## 9. Evaluation (prospective)

What the experimental section intends to establish, against whom,
and what would count as failure. Preliminary anchors exist from the
v1 campaign (212 synthetic models: verdict parity 212/212 against a
monolithic oracle, 56/56 certificates accepted by the independent
checker, zero budget violations; `experiments/cegar/`); the section
below is the campaign the paper wants.

**Questions.**

- **Q1 Correctness at scale**: verdict parity with monolithic
  explicit checking wherever the monolith fits in budget; every
  violation replayed concretely; every certificate independently
  checked. (Not a benchmark — an oracle discipline the whole
  campaign runs under.)
- **Q2 The cone emerges**: on properties touching a bounded region
  of a large model, the final profile leaves the untouched leaves
  chaotic, and the abstract product stays near the monitor's size
  while the concrete product grows. Failure mode to report honestly:
  properties whose enforcement is global (the client–server pole,
  §8), where the loop pays overhead for nothing the monolith doesn't
  already give — except the certificate.
- **Q3 Symmetry leverage**: refinement cost per *behavior*, not per
  instance, on models with replicated components; the certificate
  carries one obligation per behavior. Already measured flat on the
  synthetic ring (4 counterexamples, N = 2..64); the question at
  scale is whether real corpora expose enough byte-equal leaves —
  which is itself a finding about the front ends (currification
  quality), reported either way.
- **Q4 Budget adherence**: counterexamples spent vs the T3 bound,
  distributions not just maxima; how far below `Σ N(L_i)` the
  property lets the loop stop.
- **Q5 Certificate economics**: checker time vs verification time
  (expected orders of magnitude apart); certificate size vs model
  size; incremental re-check after a leaf edit (Remark 5.3) vs
  re-verification from scratch.

**Corpora.** (i) The synthetic families (clients, ring, random
grids) for controlled scaling and ablations — already in place.
(ii) BEEM/DVE models through the repository's DVE front end:
processes as leaves, channel/variable synchronizations as events;
the separable fragment covers the pure-synchronization subset, and
the fraction it covers is itself a reported number. (iii) MCC/NUPN
Petri corpora: unit trees as shapes, places grouped by units as
leaves. Properties: the corpora's safety/invariant properties, plus
deliberately local probes (mutual exclusion of a pair inside a large
net) to measure Q2 at controlled locality.

**Comparators.**

- **Monolithic explicit**: the in-repo explicit engine (same
  front-end, same event semantics — the clean like-for-like), and
  the loop's own `mono` walker as the parity oracle. External
  explicit checkers of the BEEM ecosystem are reference points, not
  the primary baseline: cross-tool semantics gaps make parity claims
  muddy, and the honest comparison is same-substrate.
- **Symbolic hierarchical**: the libDDD/ITS-tools lineage
  (thierrymieg2021 in the library), on the shared corpora — the
  house baseline this repository exists to challenge, run
  bounded-or-skipped per the working rules. Expectation: symbolic
  saturation wins raw state-counting on regular structure; the
  loop's case is local properties, certificates, and regression.
- **The learned assume-guarantee line**: L\*-learned assumptions for
  compositional verification (Cobleigh–Giannakopoulou–Păsăreanu and
  the studies questioning whether learned AG ever beats monolithic —
  the "breaking up is hard to do" line; *to be acquired into
  `papers/` before any citation is made*). This is the comparison
  that frames the paper: their assumption is one interface DFA
  learned against the rest-of-system as teacher — equivalence
  queries cost a product; ours is n leaf contracts with leaf-local
  teachers, canonical at every rung, interned across instances, with
  an exportable certificate. The prediction to test: their negative
  result (learning overhead swamps compositional savings) is a
  property of *where the teacher lives*, and moving every oracle
  inside the leaf flips the economics on symmetric and
  property-local instances — and where it does not flip, T3 bounds
  the loss.
- **Non-learning structural abstraction on the same corpora**:
  MCC-style reduction disciplines (berthomieu2018 counting-marking
  reductions, amat2022 polyhedral abstraction, in the library) as
  representatives of "abstract by rewriting the model, verify the
  reduct". Not head-to-head on speed — different deliverables — but
  on what survives the run: their reducts certify via
  per-transformation proofs; our certificate is checkable by a
  program the size of Remark 5.2. A qualitative comparison the
  paper should make explicit.

**Metrics.** Verdict + parity; counterexamples vs budget; rung
profile (chaotic/intermediate/exact per entry) as the emergent cone;
abstract vs concrete states walked; wall time; certificate size,
check time, and re-check time after an edit; interning multiplicity
(instances per entry). All runs seeded, all records TSV-committed,
timeouts are rows.

**Stated expectations** (falsifiable, to be printed next to the
data): flat refinement in N on replicated-component models (Q3);
abstract product within a small factor of the monitor on
locality-controlled probes while the concrete product grows without
bound (Q2); certificate checking at least two orders of magnitude
under verification (Q5); and at least one corpus slice where the
loop *loses* wall time end to end (the §8 overhead pole) — reported
as such, with the certificate as the compensating deliverable.

---

## 10. Deferred, by name — the next relaxations, in order

1. **Interior contracts.** A classifier at any node over `A_ν`,
   certified against `L_ν` by a subtree-local product; replay's
   descent then stops at the highest certified node that rejects,
   and obligations attach to interior nodes of the certificate tree.
   The new question is the size of `L_ν`'s canonical classifier
   under hiding — the projection exposure, priced before paid or not
   at all.
2. **Discovered alphabets; the bracket.** Dropping separability:
   query/case events compile to finite `(event, residual)` families;
   the cut decomposition survives over residual-extended alphabets,
   but alphabets grow mid-run — hypotheses must default unseen
   letters to chaos, and certification must be re-established on
   growth. MAT learning tolerates alphabet extension; the
   bookkeeping is real and belongs to its own draft.
3. **The ω lift.** Classifier → syntactic invariant; word executions
   → lasso evaluations; the walk threads acceptance. The three verbs
   (membership, right action, certification) survive; the new
   obligation is ω-certification — inclusion of the leaf's
   ω-behavior in the hypothesis's — again local.
4. **Symbolic walks.** Step 1 is reachability over a product of
   small tables; when indices grow, that walk is what hierarchical
   decision diagrams are for. Nothing above assumed explicitness
   except the walk.
5. **Refinement-heavy corpora for policy measurement.** The v1
   random grid is violation-dominated: witnesses are real on round
   one and the policy knobs of Remark 4.5 never diverge (300 runs,
   no separation). Measuring them needs generators with controlled
   spurious-chain depth — a corpus-design question, queued with the
   evaluation.

---

*Coda.* Three old ideas — CEGAR's refinement discipline,
assume-guarantee's local obligations, MAT learning's class-splitting
— arranged so that each supplies what the others lacked: the
composition names the culprits (CEGAR loses its heuristic step), the
learner is the refiner (refinement gains a canonical target and a
budget), and certification is local (the equivalence oracle loses
its cost). On a shape, in the separable fragment, blame is a
descent, abstractions are canonical objects at every rung, the leaf
interface is two oracles, and finiteness is one theorem's
hypothesis, not the architecture's. One round of implementation has
already paid the discipline back twice: the certificate checker
caught the loop's only soundness bug, and the measured ring/clients
poles turned §8's promise into two numbers. What the loop knows, it
proves; what it spends, it counts; what it claims, a smaller program
checks.
