# Certified Component Abstraction on a Shape

*Working draft 2. The shadow of a paper. Standalone: everything used is
stated; what is imported is imported in §2 and nowhere else. Draft 1
worked over a flat Arnold–Nivat product with DFA hypotheses and finite
components; this draft relaxes carefully, one hypothesis at a time —
the setting becomes the shape calculus's separable fragment, hypotheses
become representation-free classifiers canonical at every rung, and
finiteness of components retreats from an assumption of the
architecture to the hypothesis of one theorem.*

---

## 0. Stance, and the relaxation ledger

The object of study is a verification loop over a system given as a
**shape**: a tree whose leaves hold components and whose events are
product-shaped, each interned at the unique node it is local to. Each
leaf carries, at any moment, a **certified over-approximation** of its
behavior — a finite classifier proved, by a leaf-local check, to accept
every trace the leaf has. The loop model-checks the property against
the classifiers, replays abstract witnesses by a descent of the shape
in which membership executions name their own culprit, refines the
culprit with a MAT-style learner, and terminates with either a
concretely validated violation or a compositional certificate laid out
on the shape, checkable without the loop.

Relaxed relative to draft 1, each deliberately and separately:

- **The flat product becomes the shape.** Composition is by the tree;
  the Arnold–Nivat conjunction survives as the *image of the shape at
  each cut* (Proposition 1.6), which is where localization comes from.
- **Automata become classifiers.** A hypothesis is a finite right
  congruence given by its canonical class table — no accepting device
  is ever a separate artifact; the "automaton" is the congruence's
  right action, which is not a representation choice but the structure
  the congruence carries. Consequence: **every rung is canonical**, not
  only the exact one, and interning works mid-flight (§3).
- **Finiteness becomes a teacher contract.** What a leaf owes is two
  oracles — executable membership, and decidable-or-loud certification.
  Finite leaves discharge both by walks and get the linear budget (T3);
  well-structured unbounded leaves discharge both by execution and
  coverability and get a sound semi-algorithm complete for violations
  (§6). The architecture never mentions finiteness; one theorem does.

Still assumed, deliberately, with the relaxation that would remove each
named in §8:

- **Fixed alphabets; the separable fragment.** Every event is given as
  a product of per-leaf letters — a condition checked statically on the
  input. Query/case events, whose compiled residuals *discover* their
  exchange alphabet, are the next relaxation, not this one.
- **Safety.** Properties are bad-prefix monitors over events. The ω
  lift is a swap of the classifier notion (§2, Deferral 2.6; §8), not a
  change of loop.
- **Explicit walks.** The abstract product is walked state by state.
- **The shape is given.** Its choice is a question about the model's
  dependency structure, orthogonal here — but the shape is load-bearing
  in one new way: event *locality* is defined by where the shape
  interns each event, so "culprit" has a well-posed address because the
  shape gives it one.

---

## 1. Setting: shapes, events, behaviors

**Definition 1.1 (shape, leaves).** A shape `V` is a binary tree; write
`leaves(ν)` for the leaves below a node `ν`, and cuts for the internal
nodes. Each leaf `i` imports a theory with a **fixed finite alphabet**
`Σ_i` — letter names are position-relative (currified): two leaves
importing the same local theory with the same local code are, as
objects, one code. For this draft a leaf's behavior is the trace
language `L_i ⊆ Σ_i^*` of a finite deterministic LTS
`C_i = (Q_i, Σ_i, q_i^0, δ_i)`, `δ_i` partial: prefix-closed, nonempty,
executable. (§6 widens the leaf class; nothing before §6 depends on
finiteness except where said.)

**Definition 1.2 (events, locality).** A finite event set `E` is given;
each `e ∈ E` has support `supp(e) ⊆ leaves(V)` and a letter
`e_i ∈ Σ_i` per supported leaf — an event *is* a product of leaf
letters, which is the **standing separability assumption** of this
draft, checked statically on the input. The **top** of `e` is the least
common ancestor of `supp(e)`: the unique node whose cut `e` crosses, or
the leaf itself for singleton support. Every event is interned at its
top; that node is the event's *locality*.

**Definition 1.3 (restriction, projection).** For a node `ν` and event
`e` with `supp(e) ∩ leaves(ν) ≠ ∅`, the restriction `e|_ν` keeps the
supported letters inside `ν`. The **visible alphabet** of `ν` is
`A_ν = { e|_ν : e ∈ E, supp(e) ∩ leaves(ν) ≠ ∅ }`; at the root,
`A_V = E`. The projection `π_ν : A_μ^* → A_ν^*` (for `ν` below `μ`) is
the monoid morphism restricting event by event and erasing events with
no support in `ν`. Projections compose: `π_ν ∘ π_μ = π_ν`.

**Definition 1.4 (behavior of a node).** By recursion on the shape:
`L_i` at a leaf; at a cut `ν` with children `l, r`,

```
L_ν  =  { v ∈ A_ν^*  :  π_l(v) ∈ L_l  and  π_r(v) ∈ L_r }.
```

The **system behavior** is `L_V` at the root.

**Proposition 1.5 (leaf form).** `L_ν = { v ∈ A_ν^* : π_i(v) ∈ L_i` for
every leaf `i ∈ leaves(ν) }`.

*Proof.* Induction on the tree, using composition of projections: the
conjunction over leaves splits as the conjunction over the two
children's leaves. ∎

**Proposition 1.6 (the cut decomposition).** At every cut, `L_ν` is the
Arnold–Nivat synchronized product of its children's behaviors: events
interned at `ν` synchronize both children through their restrictions;
events interned strictly inside a child pass to that child unchanged
and are erased on the other — they *skip* it, wholesale, by
Definition 1.3.

*Proof.* Restatement of Definition 1.4 with Definition 1.2's locality:
`top(e) = ν` iff `e` has support on both sides of the cut. ∎

The proposition is trivial by design, and the design is the content:
the flat conjunction of draft 1 is recovered *at every cut at once*,
which is what makes blame a descent (§4) rather than a global analysis.
Bracketing does not change `L_V` (Proposition 1.5 mentions no cut);
what the tree adds is an address for every event and every failure.

**Definition 1.7 (property).** A safety property is a complete DFA
`A = (M, E, m_0, δ_A, Bad)` over the root alphabet. A **violation** is
`w ∈ L_V` with `δ_A(m_0, w) ∈ Bad`; the property **holds** if none
exists.

**Remark 1.8 (execution is linear).** Membership of a given word at any
node is not a search: fire it. Leaves are deterministic, so
`π_i(v) ∈ L_i` is one execution of cost `|π_i(v)|`, and membership at a
composite node is the simultaneous firing of the supported leaves. A
witness is data; checking it is running it.

---

## 2. Imports

Everything taken from elsewhere, stated as used.

**Fact 2.1 (Myhill–Nerode).** For `L ⊆ Σ^*`, let `u ~_L v` iff
`∀x. (ux ∈ L ⟺ vx ∈ L)`. This is a right congruence; `L` is regular
iff its index `N(L)` is finite; the quotient is the unique coarsest
right congruence saturating `L`.

**Fact 2.2 (prefix-closed targets).** If `L` is prefix-closed, then
`u ∉ L` implies `uΣ^* ∩ L = ∅`: negative information is
extension-closed, and `~_L` has a single **dead** class, absorbing
under the right action.

**Remark 2.3 (the index is small).** If `L` is the trace language of a
deterministic device with `m` states, then words reaching the same
state are `~_L`-equivalent, so `N(L) ≤ m + 1`.

**Fact 2.4 (MAT learning).** A minimally adequate teacher answers
membership queries and receives counterexamples (words the current
hypothesis misclassifies). A learner in the L* family maintains a
finite-index right congruence presented by representatives and
experiments, and guarantees: (F1) representatives are pairwise
distinguished by concrete experiments, hence pairwise
`~_L`-inequivalent, so the hypothesis index never exceeds `N(L)`;
(F2) processing a counterexample strictly increases the index; hence
(F3) at most `N(L)` counterexamples are ever processed, from all
sources, over the learner's life. Hypotheses are **not** monotone
across refinements — a rebuilt congruence may re-admit a previously
rejected word — and nothing below pretends otherwise. (Angluin; the
Rivest–Schapire handling; any member of the family with F1–F2.)

**Provenance 2.5 (the calculus).** The setting of §1 is the separable
fragment of the hierarchical shape calculus: events are product terms,
locality is interning at the top, skipping is the free transmission of
`id`, and currified leaf codes are position-relative. The query/case
bracket — which compiles a non-separable criterion into finitely many
`(event, residual)` pairs and thereby *discovers* an exchange
alphabet — is exactly what the standing assumption of Definition 1.2
excludes, and is the announced next relaxation (§8). This note is
written to need none of the calculus's theorems, only its setting.

**Deferral 2.6 (the ω target).** For liveness, the classifier of §3 is
replaced by the syntactic invariant of an ω-language — the finite
algebra classifying lassos, with its own canonicity and
lasso-membership evaluation, developed in the invariant line of work.
The loop of §4 consumes classifiers through three verbs only —
membership, right action, certification — so the lift is a swap of the
§3 notion, not a rewrite; §8 states what changes. This note stays with
safety so that every proof it claims, it contains.

---

## 3. Classifiers, certification, and the ladder

No automata. A hypothesis is a congruence, presented canonically; the
device that walks it is the structure the congruence already carries.

**Definition 3.1 (classifier).** A **classifier** over `Σ` is a
finite-index right congruence `≈` given by its **canonical class
table**: classes named by their shortlex-least representatives, the
right action `[u]·a = [ua]` tabulated, together with a set of **live**
classes containing `[ε]`, closed downward under the prefix reading
(a class reachable only through dead classes is dead) and with dead
classes absorbing. Its language
`L(≈) = { u : [u] is live }` is then prefix-closed. Serialization of
the table is the classifier's identity: equality is byte equality.

**Remark 3.2 (dead-absorption is free).** For a prefix-closed target
`L`, any finite-index over-approximation can be dead-closed without
losing over-approximation: if `L ⊆ L'` with `L` prefix-closed, then
`L ⊆ interior(L')` (every prefix of a member of `L` is in `L`, hence in
`L'`). So Definition 3.1's absorption costs nothing and will not be
mentioned again.

**Definition 3.3 (chaos).** The **chaotic classifier** `⊤_Σ` is the
one-class table: index 1, everything live, `L(⊤_Σ) = Σ^*`. It is the
canonical classifier *of* `Σ^*` — the bottom rung is not a degenerate
device but the exact object of the trivial language, which is why the
ladder below is uniform.

**Definition 3.4 (certification).** A classifier `H_i` is **certified**
for leaf `i` if the inclusion `L_i ⊆ L(H_i)` has been established by
the leaf-local product of `C_i` with the class table: search for a
reachable pair `(q, d)` with `q ∈ Q_i` and `d` dead. Unreachable: the
inclusion holds. Reachable along `u`: return `u ∈ L_i \ L(H_i)`, a
**positive counterexample**. Cost `O(|Q_i| · index(H_i) · |Σ_i|)`,
leaf-local.

**Definition 3.5 (the teacher contract).** A leaf theory owes the loop
two oracles and nothing else:

- **membership**, executable: given `u`, decide `u ∈ L_i` by running
  it (Remark 1.8);
- **certification**, decidable or loud: given a classifier, establish
  `L_i ⊆ L(H_i)` or return a positive counterexample; where the
  underlying check is only semi-decidable, divergence must be a
  visible divergence, never a clamp and never a wrong answer.

Finite LTS leaves discharge both by Remark 1.8 and Definition 3.4. §6
exhibits a second instance. The contract is the entire interface
between the loop and the leaf class; every theorem below is proved
against it.

**Lemma 3.6 (white-box teachers are local).** Under the contract, each
leaf supports a minimally adequate teacher whose every answer is
leaf-sized: membership is one execution, and the counterexample channel
is fed by certification (positive) and by the loop's replay (negative,
T2). No oracle touches another leaf or any product. ∎

The reading from draft 1 stands and sharpens: learning here is not
compensating for missing access — the leaves are white boxes — it is
*choosing what to represent*, and the choice is driven by the property
through the counterexamples the loop generates.

**Definition 3.7 (profile, rungs — canonical throughout).** A
**profile** assigns each leaf a certified classifier. Rungs, by index:
chaos `⊤_{Σ_i}` (index 1, certified for free) up to the **exact** rung,
the canonical classifier of `L_i` itself (index `N(L_i)`, which by
Fact 2.1 is the coarsest congruence saturating `L_i` — the canonical
object, not a minimal device). After every refinement the table is
re-canonicalized (leaf-scale: recompute shortlex representatives,
re-sort), so **every rung is a canonical object**: two leaves whose
current abstractions coincide as languages carry byte-equal tables,
mid-flight, not only at convergence. With currified leaf codes
(Definition 1.1), two isomorphic leaves share one classifier, one
learner, one certification — and every refinement of one is a
refinement of all (Remark 5.4).

**Lemma 3.8 (profile soundness).** If `L_i ⊆ L(H_i)` for every leaf,
then `L_V ⊆ L_V[H]`, where `L_V[H]` is Definition 1.4 with each `L_i`
replaced by `L(H_i)`.

*Proof.* Pointwise in Proposition 1.5's leaf form. ∎

---

## 4. The loop

**Algorithm 4.1.** Maintain a certified profile; initially all-chaotic.
Repeat:

1. **Search.** Walk the abstract product: states
   `(m, [u_1], …, [u_n]) ∈ M × Π_i classes(H_i)`, transitions by
   `δ_A` on the event and the right action of each supported leaf's
   table on its letter, blocked when any supported class goes dead.
   (The walk is flat; the shape has already done its work by fixing
   which leaves each event touches.) If no state with `M`-coordinate
   in `Bad` is reachable: emit the certificate of §5 and stop — the
   property holds.
2. **Replay by descent.** A bad abstract path yields `w ∈ L_V[H]`
   reaching `Bad`. Descend the shape: at a node whose subtree contains
   no support of `w`, pass — the skip of Proposition 1.6, taken
   wholesale; at a cut, recurse on both children with the projected
   words; at a leaf, execute (Remark 1.8). If every leaf execution
   completes, stop — `w` is a violation, validated by the executions.
   (Extensionally this is the set of leaf tests of Proposition 1.5;
   the descent form is stated because it is what survives when
   interior nodes carry their own contracts, §8.)
3. **Refine.** Otherwise the **culprits** are the leaves whose
   execution failed; for each culprit `i`, `u_i = π_i(w)` satisfies
   `u_i ∈ L(H_i) \ L_i` — a certified negative counterexample.
   *Refine until resolved*: repeatedly feed `u_i` to learner `i`,
   rebuild and re-canonicalize the table, re-certify (feeding back any
   positive counterexample), until the current classifier is certified
   and classifies `u_i` dead. Go to 1.

**Theorem T1 (soundness).** If step 1 finds no bad reachable state, the
property holds.

*Proof.* The profile is certified whenever used (loop invariant). By
Lemma 3.8, any violation would lie in `L_V[H]` and reach `Bad`; by
Definition 1.4 applied to the classifiers, its class-tuple run exists
in the abstract product and reaches a bad state. ∎

**Theorem T2 (localization; culprits name themselves).** For `w`
produced by step 1: (i) if every leaf execution in the descent
completes, `w` is a real violation; (ii) every leaf whose execution
fails yields `u_i ∈ L(H_i) \ L_i`, a sound negative counterexample,
certified by the failed execution itself; (iii) if `w` is spurious, at
least one leaf fails; and the descent reaches every culprit while
skipping, without inspection, every subtree `w` does not touch.

*Proof.* (i) Completion means `π_i(w) ∈ L_i` for all leaves, so
`w ∈ L_V` (Proposition 1.5) and `w` reaches `Bad` by construction.
(ii) `w ∈ L_V[H]` gives `π_i(w) ∈ L(H_i)`. (iii) `w ∉ L_V` means some
leaf projection is not a trace. Skipping is Definition 1.3: a subtree
without support receives `ε`. ∎

Blame is executions, organized by the tree: no simulation of the
abstract counterexample against a concrete product, no cores, no
heuristics — the composition is a conjunction at every cut
(Proposition 1.6), so failure is locatable by descent, and the cost of
locating it is proportional to what the witness touches, not to the
system.

**Theorem T3 (progress and termination — the finite instance).** With
finite LTS leaves: every execution of step 3 strictly increases
`Σ_i index(H_i)`; throughout, `index(H_i) ≤ N(L_i) ≤ |Q_i| + 1`
(F1, Remark 2.3); hence at most `Σ_i (|Q_i| + 1)` counterexamples are
ever processed, from both sources combined, and Algorithm 4.1
terminates with a verdict.

*Proof.* Step 3 processes at least one genuinely misclassified word
(T2(ii) for negatives; Definition 3.4 for positives); F2 gives the
strict increase; F1 with Remark 2.3 the cap; the *until resolved* inner
loop burns the same budget, so it too halts. Between refinements,
search and replay are finite. ∎

**Remark 4.2 (recurrence is possible and harmless — here).** Because
hypotheses are not monotone (Fact 2.4), a refuted witness may reappear
in a later round. Each reappearance is again a misclassification and
burns budget, so T3 is unaffected; draft 1's claim that resolution
banishes a witness permanently was an overclaim and is withdrawn. Where
there is no budget to burn, non-recurrence must be *manufactured* — the
negative store of §6, which is needed there and merely optional here.

**Remark 4.3 (the bound, read).** `Σ_i (|Q_i| + 1)` is linear in the
model: refinement can never do worse than reconstructing every leaf's
canonical classifier, one class at a time, and it does so only where
the property insists. The exponential lives where it always lived — in
step 1's walk, `|M| · Π_i index(H_i)` at worst — but over *classifier*
indices: `1` for every leaf still chaotic. The cone of influence is not
computed; it is the set of leaves whose rung ever left chaos, read off
the final profile.

**Remark 4.4 (both verdicts are self-evident).** "Holds" carries the
§5 certificate; "violation" carries `w` and its leaf executions,
re-runnable by anyone. Neither asks for trust in the learner, the
search order, or the refinement policy — which culprit first, whether
to batch, when to jump a leaf straight to exact: all policies satisfy
T1–T3, so choosing among them is measurement (§9), not proof.

---

## 5. Certificates on the shape

**Definition 5.1 (certificate).** `K = ⟨ (H_i)_{i}, Inv ⟩`: one
classifier per leaf, and a finite set `Inv` of abstract states of the
step-1 product. Obligations:

- **(L_i)**, one per leaf, attached to the leaf: `L_i ⊆ L(H_i)`,
  checked by the leaf-local product of Definition 3.4;
- **(G1)** the initial abstract state is in `Inv`; **(G2)** `Inv` is
  closed under every event (right actions on supported tables, `δ_A`
  on the monitor); **(G3)** no state of `Inv` is bad.

**Theorem T5 (certificates are sound, independently of provenance).**
If all obligations hold, the property holds.

*Proof.* (G1)–(G3): `Inv` is an inductive invariant of the abstract
product avoiding `Bad`, so no abstract violation exists; (L_i) for all
leaves and Lemma 3.8: `L_V ⊆ L_V[H]`; conclude as in T1. No step
refers to how `K` was produced. ∎

**Remark 5.2 (the checker is small).** Two kinds of walk and a closure
scan; no learner, no search heuristics, no shape analysis beyond
reading supports. A verification of a large model certifies as `n`
independent leaf lemmas plus one small induction — the
assume-guarantee shape, as data, laid out on the tree.

**Remark 5.3 (regression).** Edit leaf `i`; re-check (L_i) alone. Pass:
the entire proof stands unread — the new leaf still meets the contract
under which the global argument was made. Fail: the check returns the
offending trace, which is a resumption point for the loop. Contracts,
operationally.

**Remark 5.4 (interning, now everywhere).** Canonical-at-every-rung
(Definition 3.7) upgrades draft 1's convergence-only interning:
isomorphic leaves (byte-equal currified codes) share one classifier and
one (L) obligation throughout the run, and a witness spurious at one
instance refines all `k` siblings in lockstep — the certificate for a
symmetric system carries one leaf lemma per *behavior*, not per
instance.

---

## 6. Room left: well-structured leaves

Not the focus; recorded to fix the interface it exercises. The teacher
contract (Definition 3.5) never mentioned finiteness. Take a leaf whose
behavior is the trace language of a well-structured transition system —
an unbounded Petri net place structure under a wqo with effective
pred-basis. Then:

- **membership** is still one execution (fire the word);
- **certification** is the reachability, in `C_i ×` table, of the
  upward-closed set `{ (m, d) : d dead }` — a **coverability**
  question, decidable for WSTS by backward saturation (Abdulla et al.;
  Finkel–Schnoebelen). Component-local, expensive, decidable: the
  contract's "decidable or loud", discharged on the decidable side.

T1, T2, T5 hold verbatim — their proofs used the contract, not the
walks. T3 fails, and must: `N(L_i)` is infinite already for one
unbounded counter (`a^n b^{≤m≤n}`), so there is no budget.

**Proposition 6.1 (sound semi-algorithm, complete for violations).**
Augment the loop with (a) a **persistent negative store**: each leaf
keeps every certified non-trace `d` ever produced by replay, and its
effective abstraction is the classifier meet of the learner's table
with the canonical classifier of `Σ_i^* ∖ ⋃ d·Σ_i^*` (finite: a
prefix-tree quotient; the meet is again a classifier, and
over-approximation is preserved by Fact 2.2); and (b) **length-fair
search** in step 1. Then the loop never reports a false verdict, and if
a violation exists, it is found.

*Proof.* Soundness: unchanged (certification is re-established on every
effective abstraction). Completeness: let `w` be a shortest violation,
`|w| = ℓ`. The store makes refutation permanent: a witness once refuted
has some `π_i` in leaf `i`'s store and is excluded from every later
`L_V[H]`. Words of length `≤ ℓ` are finitely many; each spurious one is
refuted at most once; a real witness is never excluded (its projections
are traces, and the store holds only certified non-traces). Length-fair
search therefore reaches `w` after finitely many rounds, and replay
validates it. ∎

The rung ladder specializes pleasantly: for a single unbounded counter,
the certified classifiers distinguishing values up to `k` with a
chaotic tail are exactly the classical counter abstractions —
rediscovered as rungs, refined on demand, certified by coverability.
This subsection is the whole of what this draft says about
unboundedness; making it a focus is a different note.

---

## 7. Cost shape, honestly

Cheap: replay (executions, proportional to the witness's support,
parallel over leaves); certification (leaf-local products);
refinement (linearly budgeted, T3); certificate checking (Remark 5.2);
canonicalization (leaf-scale sorting).

Not cheap: step 1's walk, `|M| · Π_i index(H_i)` at worst — the
irreducible cost of composition, honestly priced: over classifier
indices, size 1 wherever the property has not probed, exact only where
it insisted; the degenerate case where everything is driven exact walks
roughly the minimized monolithic product plus learning overhead. The
loop wins when the property is local, ties with overhead when it is
not, and T3 caps the cost of finding out.

One exposure deferred rather than hidden: classifiers here range over
each leaf's **full** alphabet, and interior nodes carry no contracts of
their own. True interface contracts — a classifier at a cut, over the
cut's visible alphabet, with the subtree's internal events hidden —
are where hiding meets abstraction and where projection can blow up;
Definition 1.4 is already stated at every node precisely so that this
extension changes the *placement* of hypotheses and not one definition.
Deferred by name in §8.

---

## 8. Deferred, by name — the next relaxations, in order

1. **Interior contracts.** A classifier at any node over `A_ν`,
   certified against `L_ν` by a subtree-local product; replay's
   descent (step 2) then stops at the highest certified node that
   rejects, and obligations attach to interior nodes of the
   certificate tree. The new question is the size of `L_ν`'s canonical
   classifier under hiding — the projection exposure, priced before
   paid or not at all.
2. **Discovered alphabets; the bracket.** Dropping the standing
   separability assumption: query/case events compile to finite
   `(event, residual)` families, the cut decomposition survives over
   residual-extended alphabets, but alphabets *grow* during the run —
   hypotheses must default unseen letters to chaos, and certification
   must be re-established on growth. MAT learning tolerates alphabet
   extension; the bookkeeping is real and belongs to its own draft.
3. **The ω lift.** Classifier → syntactic invariant; word executions →
   lasso evaluations; the walk threads acceptance. The loop's three
   verbs (membership, right action, certification) survive; the new
   obligation is ω-certification — inclusion of the leaf's ω-behavior
   in the hypothesis's — again local.
4. **Symbolic walks.** Step 1 is reachability over a product of small
   tables; when indices grow, that walk is what hierarchical decision
   diagrams are for. Nothing above assumed explicitness except the
   walk.
5. **Shape choice.** Orthogonal by fiat; the cost model it should
   optimize is visible in T3 and §7.

---

## 9. Oracles first

No stage graduates without its oracle named:

- **Verdict parity** with monolithic explicit-state checking on every
  model small enough, both verdicts, violations replayed concretely.
- **Round assertions**: after step 3, `Σ index(H_i)` strictly
  increased; every classifier certified and canonical (byte-idempotent
  under re-canonicalization); the culprit's `u_i` classified dead.
- **Budget assertion** (finite leaves): total counterexamples
  `≤ Σ (|Q_i| + 1)`; violation indicts learner or teacher, and says
  which.
- **Interning assertion**: byte-equal leaves carry pointer-equal
  classifiers at all times.
- **Independent checker**: a separate program, no shared code, run on
  every emitted certificate.

*Coda.* Draft 1 arranged three old ideas — CEGAR's discipline,
assume-guarantee's local obligations, MAT's class-splitting — so that
each supplied what the others lacked. This draft adds the fourth voice
and the claim sharpens: on a shape, in the separable fragment, blame is
a descent, abstractions are canonical objects at every rung, the leaf
interface is two oracles, and finiteness is one theorem's hypothesis
rather than the architecture's. What was relaxed was relaxed one
hypothesis at a time, and each relaxation either preserved a theorem's
proof verbatim (T1, T2, T5) or replaced it by the honest weaker
statement (T3 → Proposition 6.1). That discipline — relax, and say
exactly which proofs noticed — is the method this program keeps.
