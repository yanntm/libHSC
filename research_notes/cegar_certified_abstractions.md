# Certified Component Abstraction: a CEGAR Whose Refiner Is a Learner

*Working draft 1. The shadow of a paper. Standalone: everything used is
stated; what is imported is imported in §2 and nowhere else.*

---

## 0. Stance and scope

The object of study is a verification loop over a system given as a
**shape**: a tree whose leaves are components. Each component carries, at
any moment, a **certified over-approximation** of its behavior — an
automaton proved, by a component-local check, to accept every trace the
component has. The loop model-checks the property against the
abstractions, replays abstract witnesses by membership queries that name
their own culprit, and refines the culprit with a MAT-style learner. The
loop terminates with either a concretely validated violation or a
compositional proof certificate whose checker needs neither the learner
nor the search.

Four claims, each a theorem below:

1. **Soundness is a local invariant, not a global argument** (T1): every
   abstraction in play is certified against its component by a
   component-sized inclusion check; the global claim follows by
   composition of one-line lemmas.
2. **Spurious witnesses name their culprit by construction** (T2): replay
   is one membership execution per touched component; no
   abstract-counterexample simulation, no heuristics.
3. **Progress is class-splitting, and it is linearly bounded** (T3):
   every counterexample — spurious-replay negative or certification
   positive — strictly grows some hypothesis, and hypotheses cannot
   outgrow the component's Nerode index, which for a deterministic
   component is at most its state count plus one. The whole loop is
   bounded by the *sum* of component sizes, not their product.
4. **The proof is an exportable object** (T5): n local inclusion
   obligations plus one small induction, checkable independently,
   in parallel, and — after a component is edited — incrementally.

Assumed from outside, deliberately: the shape (its choice is a question
about the model's dependency structure, orthogonal to this note); explicit
state everywhere (no decision diagrams — the abstract product is walked
on the fly); safety properties (the ω lift is scoped in §8, and imports
its canonical target from the invariant line of work; nothing in the loop's
architecture changes, but the proofs here stay elementary by staying
finite); white-box components (the system is being model-checked, so its
parts are executable; what learning buys is not access but *smallness*).

---

## 1. Setting

**Definition 1.1 (components).** A component is a finite deterministic
LTS `C_i = (Q_i, Σ_i, q_i^0, δ_i)` with `δ_i` partial. Its behavior is
the trace language `L_i ⊆ Σ_i^*`: the words along which `δ_i` is
defined from `q_i^0`. `L_i` is nonempty and prefix-closed.
Determinism is for convenience of counting (Remark 2.3); traces of a
nondeterministic component serve verbatim at the price of a looser bound.

**Definition 1.2 (shape and synchronization).** A shape `V` is a binary
tree with leaves `1..n`. A finite **event set** `E` is given; each
`e ∈ E` has a support `supp(e) ⊆ {1..n}` and a letter `e_i ∈ Σ_i` for
each `i ∈ supp(e)`. The projection `π_i : E^* → Σ_i^*` is the monoid
morphism sending `e` to `e_i` if `i ∈ supp(e)` and to `ε` otherwise.
The **system behavior** is

```
L_V  =  { w ∈ E^*  :  π_i(w) ∈ L_i  for every i }.
```

This is the synchronized product in the sense of Arnold–Nivat: a global
word is legal iff every component accepts its own shadow of it.

**Proposition 1.3 (states and words agree).** Let `P` be the product
automaton on `Π_i Q_i` with `e` enabled in `(q_1,…,q_n)` iff
`δ_i(q_i, e_i)` is defined for every `i ∈ supp(e)`, moving exactly the
supported coordinates. Then `traces(P) = L_V`.

*Proof.* Induction on `|w|`. The run of `P` on `w` exists iff at each
step every supported `δ_i` is defined, iff each `π_i(w)` is a trace of
`C_i` — unsupported events leave `q_i` fixed and contribute `ε` to
`π_i(w)`. ∎

**Remark 1.4 (what the shape is for).** `L_V` does not depend on the
bracketing: the product is associative, and the theorems below never
mention the tree. The shape earns its keep three times, all economic:
replay descends it (a witness is projected cut by cut, so a subtree
containing no support of the suffix under scrutiny is skipped wholesale);
certificates are laid out on it (obligations attach to leaves, §5); and
interning acts on it (isomorphic subtrees share hypotheses and
obligations, Remark 5.4). Choosing the shape well is real and is not this
note's question.

**Definition 1.5 (property).** A safety property is a complete DFA
`A = (M, E, m_0, δ_A, Bad)` over the event alphabet. A **violation** is
`w ∈ L_V` with `δ_A(m_0, w) ∈ Bad`. The property **holds** if there is
no violation. (Bad-prefix monitors for regular safety; liveness is §8.)

---

## 2. Imports

Everything this note takes from elsewhere, stated as used.

**Fact 2.1 (Myhill–Nerode).** For `L ⊆ Σ^*` let `u ~_L v` iff
`∀x. (ux ∈ L ⟺ vx ∈ L)`. `L` is regular iff `~_L` has finite index
`N(L)`, and the minimal complete DFA `Min(L)` has exactly `N(L)` states
and is unique up to isomorphism — literally unique once states are named
canonically (shortlex-least access words), which is the naming used for
interning in §5.

**Fact 2.2 (prefix-closed shape of the target).** If `L` is prefix-closed
then `u ∉ L` implies `uΣ^* ∩ L = ∅`; consequently `Min(L)` has a single
rejecting state, an absorbing sink, and every other state accepting.
Negative information is extension-closed: one certified non-trace
condemns its whole cone.

**Remark 2.3 (the index is small).** If `L` is the trace language of a
deterministic automaton with `m` states, then `u ~_L v` whenever `u, v`
reach the same state, so `N(L) ≤ m + 1` (the states, plus the sink).
This single inequality is what makes the termination bound of T3 linear
in the model rather than exponential.

**Fact 2.4 (MAT learning).** A minimally adequate teacher for `L` answers
membership queries (`u ∈ L`?) and counterexample submissions (a word the
current hypothesis misclassifies). A MAT learner in the L* family
maintains a hypothesis DFA `H` and guarantees: (F1) the states of `H`
are pairwise distinguished by concrete experiments, hence pairwise
Nerode-inequivalent, hence `H` never has more than `N(L)` live states;
(F2) processing a counterexample yields a hypothesis with strictly more
states. Consequently (F3) at most `N(L)` counterexamples are ever
processed, over the learner's whole life, from all sources combined.
(Angluin's L*; the Rivest–Schapire counterexample handling; any member
of the family with F1–F2 serves. The learner's internals are otherwise
not this note's concern.)

**Deferral 2.5 (the ω target).** For liveness the canonical target of
learning is not `Min(L)` but the syntactic invariant of an ω-language —
the finite algebra classifying lassos — whose construction,
canonicity, and lasso-membership evaluation are developed in the
invariant line of work ([SωS] and companions). §8 states exactly what
changes in the loop under that import: the replay walks lassos and the
canonicity import upgrades; the architecture — certified
over-approximation, self-naming culprits, split-bounded progress,
exportable certificates — does not move. This note stays with safety so
that every proof it claims, it contains.

---

## 3. Certified over-approximation

**Definition 3.1 (hypothesis, optimistically completed).** A hypothesis
for component `i` is a complete DFA `H_i` over `Σ_i`. The learner's
partial row automaton is completed **optimistically**: every transition
the observations do not determine goes to a distinguished chaotic state
`⊤` with `δ(⊤, a) = ⊤` for all `a`, `⊤` accepting. Rejection can only
occur through explored structure; ignorance defaults to permission. The
**chaotic hypothesis** is `⊤` alone: one state, `L(⊤) = Σ_i^*`.

Optimistic completion is a convention, not a soundness argument: a wrong
state merge can still route a genuine trace to a rejecting state. What
makes an abstraction usable is the next definition, and only it.

**Definition 3.2 (certification).** A hypothesis `H_i` is **certified**
for `C_i` if the inclusion `L_i ⊆ L(H_i)` has been established by the
product walk on `C_i × H_i`: search for a reachable pair `(q, h)` with
`q` a state of `C_i` and `h` rejecting in `H_i`. If none is reachable the
inclusion holds and the walk is the certificate; if one is reached along
`w`, then `w ∈ L_i \ L(H_i)` is returned — a **positive
counterexample**. Cost: `O(|Q_i| · |H_i| · |Σ_i|)`, component-local.

**Lemma 3.3 (white-box teachers are local and exact).** Component `i`
supports a minimally adequate teacher in which a membership query is one
execution of `C_i` (cost `|u|`) and a full equivalence query is the
certification walk of Definition 3.2 plus one containment in the other
direction if exactness is wanted. No query touches any other component
or the product. ∎

The reading matters: in a black-box setting the equivalence oracle is
the expensive fiction; here every oracle is a small concrete program.
Learning is not compensating for missing access — it is *choosing what
to represent*. A component the property never probes remains at `⊤`,
which is certified for free; a component probed shallowly stabilizes at
a hypothesis tracking only the distinctions the verification has paid
for.

**Definition 3.4 (profile, rungs).** A **profile** assigns each leaf a
certified hypothesis. The rungs, ordered by `|H_i|`: chaotic `⊤`
(1 state) ⊒ certified intermediate hypotheses ⊒ **exact** `Min(L_i)`
(`N(L_i)` states, canonical by Fact 2.1) — with the degenerate top
"concrete", i.e. `H_i = C_i` itself (trivially certified), available
when `C_i` is small enough that abstraction is not worth its keep. The
initial profile is all-chaotic and certified.

**Lemma 3.5 (profile soundness).** If `L_i ⊆ L(H_i)` for every `i`, then
`L_V ⊆ L_V[H] := { w ∈ E^* : π_i(w) ∈ L(H_i) for every i }`.

*Proof.* Pointwise from the definition of `L_V` as a conjunction of
projections. ∎

---

## 4. The loop

**Algorithm 4.1.** Maintain a certified profile; initially all-chaotic.
Repeat:

1. **Search.** Walk the abstract product `A × Π_i H_i` (explicit,
   on the fly, states materialized as reached) for a reachable state
   with `A`-coordinate in `Bad`. If none: emit the certificate of §5
   and stop — the property holds.
2. **Replay.** A bad abstract path yields `w ∈ L_V[H]` with
   `δ_A(m_0,w) ∈ Bad`. For each `i` with `supp` in `w`: execute
   `π_i(w)` on `C_i`. If every execution completes: stop — `w` is a
   violation, validated by the executions themselves.
3. **Refine.** Otherwise let the **culprits** be every `i` whose
   execution failed, and let `u_i = π_i(w)`. For each culprit
   (batched or one by one — a policy knob, not a soundness point):
   *refine until resolved* — repeatedly feed the counterexample `u_i`
   to learner `i`, rebuild `H_i`, re-certify (feeding back any positive
   counterexample the certification returns), until the current `H_i`
   is certified **and rejects `u_i`**. Go to 1.

The *until resolved* clause is deliberate: a MAT learner owes a state
per counterexample (F2), not immediate correct classification of it;
without the clause the same witness could be resubmitted. With it, each
resubmission costs the learner another state, so resolution arrives
within the budget of F3, and the witness just refuted can never be
produced by the search again.

**Theorem T1 (soundness).** If step 1 finds no bad reachable state,
the property holds.

*Proof.* The profile is certified (invariant of the loop: certification
is re-established before any hypothesis is used). By Lemma 3.5,
`L_V ⊆ L_V[H]`. Any violation `w` would lie in `L_V[H]` with
`δ_A(m_0,w) ∈ Bad`; by Proposition 1.3 applied to the abstract product
(whose components are the `H_i` and the monitor), `w` would be a trace
of `A × Π H_i` reaching `Bad` — a bad reachable state. ∎

**Theorem T2 (localization; spurious witnesses name their culprit).**
Let `w` be produced by step 1. Then:

(i) if every replay in step 2 completes, `w` is a real violation;
(ii) every `i` whose replay fails satisfies `u_i ∈ L(H_i) \ L_i` — a
sound negative counterexample for learner `i`, certified by the failed
execution itself;
(iii) if `w` is not a real violation, at least one replay fails.

*Proof.* (i) Completion of every execution means `π_i(w) ∈ L_i` for all
`i`, so `w ∈ L_V` by definition, and `w` reaches `Bad` in `A` by
construction of the search. (ii) `w ∈ L_V[H]` gives `π_i(w) ∈ L(H_i)`;
the failed execution gives `π_i(w) ∉ L_i`. (iii) `w ∉ L_V` means some
`π_i(w) ∉ L_i`, and that replay fails. ∎

The point of T2 is what it does *not* require: no simulation of the
abstract counterexample against the concrete product, no interpolation,
no unsat cores, no policy. The composition is a conjunction of
projections, so blame is a membership query — `|supp(w)|` executions,
independent, each component-sized, each returning either "real here" or
a certified training datum.

**Theorem T3 (progress and termination).** Every execution of step 3
strictly increases `Σ_i |H_i|`. Throughout the run, every `H_i` has at
most `N(L_i)` live states, and with Remark 2.3 at most `|Q_i| + 1`.
Hence the loop executes step 3 at most `Σ_i N(L_i) ≤ Σ_i (|Q_i| + 1)`
times, and Algorithm 4.1 terminates with a verdict.

*Proof.* Step 3 processes at least one counterexample: `u_i` is
misclassified by `H_i` (T2(ii)), and every positive counterexample
returned by certification is misclassified by definition. Each processed
counterexample adds a state (F2); no hypothesis exceeds `N(L_i)` live
states (F1); `N(L_i) ≤ |Q_i| + 1` (Remark 2.3). The *until resolved*
clause terminates within the same budget, since each iteration processes
a still-misclassified word. Between consecutive executions of step 3 the
search and replay are finite. So the loop halts, and it halts only in
step 1 (with T1's proof) or step 2 (with a violation validated by
execution). ∎

Read the bound. `Σ_i (|Q_i| + 1)` is *linear in the model*: refinement
can never do worse than reconstructing every component's minimized
behavior, one class at a time, and it does that only when the property
insists. The exponential lives where it always lived — in the abstract
product walk of step 1, `|M| · Π_i |H_i|` in the worst case — but it is
a product of *hypothesis* sizes, `1` for every component the property
has not probed. The cone of influence is not computed; it emerges as
the set of components whose rung ever left chaotic.

**Remark 4.2 (both verdicts are self-evident).** A "holds" verdict
carries the certificate of §5, checkable without trusting the loop. A
"violation" verdict carries `w` and its replays: the evidence is `n`
component executions, re-runnable by anyone. Neither verdict asks for
faith in the learner, the search order, or the refinement policy.

**Remark 4.3 (policy knobs, named as such).** Which culprit to refine
first when several fail (rarest, cheapest, leftmost); whether to batch
all culprits of one witness; whether to drive a heavily-hit component
straight to its exact rung rather than class by class; when to prefer
the concrete rung outright. All are instances of the same loop — T1–T3
hold for every policy — so the choice among them is a measurement, not
a proof obligation.

---

## 5. Certificates

**Definition 5.1 (certificate).** A certificate is
`K = ⟨ (H_i)_{i ≤ n}, Inv ⟩` where each `H_i` is a complete DFA over
`Σ_i` and `Inv` is a finite set of abstract states
`(m, h_1, …, h_n) ∈ M × Π_i states(H_i)`. Its **obligations**:

- **(L_i)** for each `i`: `L(C_i) ⊆ L(H_i)` — one component-local
  product walk, `O(|Q_i| · |H_i| · |Σ_i|)`;
- **(G1)** the initial abstract state is in `Inv`;
- **(G2)** `Inv` is closed under every `e ∈ E`: if `s ∈ Inv` and `e` is
  enabled at `s` in `A × Π H_i`, the successor is in `Inv`;
- **(G3)** no state of `Inv` has its `M`-coordinate in `Bad`.

**Theorem T5 (certificates are sound, independently of provenance).**
If all obligations hold, the property holds.

*Proof.* (G1)–(G3) make `Inv` an inductive invariant of the abstract
product avoiding `Bad`, so no abstract violation exists; (L_i) for all
`i` and Lemma 3.5 give `L_V ⊆ L_V[H]`; conclude as in T1. Nothing in
the argument refers to how `K` was obtained. ∎

Algorithm 4.1 emits `K` with `Inv` the reached set of its final search,
but T5 is quantified over arbitrary `K`: a hand-written profile, a
profile imported from a previous verification, a profile produced by a
different tool — all check the same way.

**Remark 5.2 (the checker is a trusted-core candidate).** The obligation
checker is a few dozen lines: two product walks and a closure scan. No
learner, no CEGAR, no search heuristics. The proof of a large
verification is `n` independent local lemmas plus one small induction —
the assume-guarantee shape, materialized as data.

**Remark 5.3 (regression).** Edit component `i`. Re-check (L_i) alone —
component-sized. If it passes, the entire proof stands, unread: the new
component still meets the contract under which the global argument was
made. If it fails, the walk returns the offending trace, which is
exactly a resumption point for the loop — refine `H_i` from the failure
and re-search. Certificates survive revision that stays within contract;
this is what "contract" should operationally mean.

**Remark 5.4 (interning).** Name hypothesis states canonically
(shortlex access words, Fact 2.1) and serialize; equality of
abstractions is byte equality. Two leaves whose components are
identical up to the alphabet bijection induced by the wiring — checked
once, concretely, by canonical minimization of the components
themselves — can *share* their hypothesis: one learner instance, one
(L) obligation for the whole class, and every refinement propagates to
all members simultaneously. A witness spurious at one instance trains
its `k` siblings. No prior CEGAR has this because no prior CEGAR's
abstractions had a canonical form to intern.

---

## 6. A worked round: two clients and a server

Small enough to check by hand; shaped to show the one phenomenon that
matters (the cone of influence emerging).

**The model.** Alphabet of global events `E = {g1, r1, g2, r2}` (grant
and release for each client). Three components:

- Server `S`, `Σ_S = E`, states `{F, B1, B2}`:
  `F —g1→ B1`, `B1 —r1→ F`, `F —g2→ B2`, `B2 —r2→ F`.
- Client `Cl_1`, `Σ_1 = {g1, r1}`, states `{I, C}`: `I —g1→ C —r1→ I`.
- Client `Cl_2` symmetric over `{g2, r2}`.

Supports: `g_i, r_i` sync `Cl_i` and `S`. Shape: `(Cl_1, (S, Cl_2))` —
or any other bracketing; Remark 1.4 applies.

**The property.** "No grant while a grant is outstanding." Monitor `M`:
states `{m0, m1, m2, bad}`; `m0 —g1→ m1 —r1→ m0`,
`m0 —g2→ m2 —r2→ m0`; any `g` from `m1` or `m2` → `bad`; releases
without grant self-loop harmlessly on `m0`.

**Round 1.** Profile all-chaotic: the abstract product is the monitor
alone. Search finds the bad path `w = g1·g2`. Replay:
`π_1(w) = g1 ∈ L(Cl_1)` ✓; `π_2(w) = g2 ∈ L(Cl_2)` ✓;
`π_S(w) = g1·g2` — execution fails at `B1` (no `g2`). **One culprit,
named by three executions totalling three steps.** The clients are never
touched again.

**Refine S.** Negative counterexample `g1·g2`. The learner splits: `ε`
and `g1` are distinguished (experiment `g2`: `g2 ∈ L_S`,
`g1·g2 ∉ L_S`), and the certification exchange (positive
counterexamples restoring, e.g., `g1·r1` and `g2·r2` to the accepted
language — the exact exchange depends on the counterexample-handling
policy and is not the point) drives the hypothesis to the three live
classes `[ε] = F`, `[g1] = B1`, `[g2] = B2` plus the rejecting sink:
`H_S = Min(L_S)`, the exact rung, certified by a `3 × 4` walk. The
property pushed the one load-bearing component to its canonical
contract and left everything else at `⊤`.

**Round 2.** Search `M × H_S` (clients still `⊤`, contributing
nothing): reachable states
`Inv = { (m0,F), (m1,B1), (m2,B2) }` — from `(m1,B1)` only `r1` is
enabled (the sink blocks `g`-moves in `H_S`), returning to `(m0,F)`;
symmetrically for `B2`. `bad` unreachable. **Emit certificate:**

- `H_1 = H_2 = ⊤` — obligations (L_1), (L_2) trivially pass;
- `H_S` as above — obligation (L_S): the `3 × 4` product walk;
- `Inv`, three tuples — (G1) `(m0,F) ∈ Inv` ✓; (G2) closure under the
  four events, twelve checks ✓; (G3) no `bad` coordinate ✓.

Total refinements: within the T3 budget of
`Σ(|Q_i|+1) = 3 + 3 + 4 = 10`, two counterexamples were spent, both on
`S`. With `k` clients instead of two, nothing changes for the clients:
all `k` share the chaotic hypothesis and the trivially-true obligation
(Remark 5.4), and the abstract product still walks
`|M| × |H_S|` states. The example is minimal to the point of parody —
the server *is* the property's enforcer, so of course refinement lands
there — but that is the phenomenon stated at its cleanest: refinement
lands where enforcement lives, and it found the place by executing
three words.

---

## 7. Cost shape, honestly

What is cheap: replay (component executions, embarrassingly parallel);
certification (component-local products); refinement (bounded linearly,
T3); certificate checking (Remark 5.2).

What is not: step 1's abstract product walk, `|M| · Π_i |H_i|` at
worst. That factor is the irreducible cost of composition and this note
does not pretend otherwise. The claim is about *which* product is
walked: hypotheses, not components — size 1 wherever the property has
not probed, exact only where it insisted. The degenerate worst case —
every component driven exact — walks approximately the minimized
monolithic product plus the learning overhead, which prices the promise
honestly: the loop wins when the property is local, ties (with
overhead) when it is not, and T3 caps how much refinement can ever be
spent finding out.

One exposure is deferred rather than hidden: hypotheses here range over
each component's **full** alphabet. True interface contracts — hiding
letters internal to a subtree — shrink the alphabets but compose
hiding with the abstraction, and projection is where sizes can blow up.
That interaction (and the read-offs that price a projection before it
is paid) belongs to the invariant line of work and to a later stage of
this program; nothing in T1–T5 anticipates it wrongly, since the
theorems never hide a letter.

---

## 8. Deferred, by name

**The ω lift.** Replace `Min(L_i)` by the syntactic invariant of the
component's ω-behavior (Deferral 2.5); witnesses become lassos;
replay becomes one lasso-membership evaluation per component — a fold
and a lookup — so T2's architecture survives verbatim; the abstract
search threads acceptance through the product (the profile machinery of
the nondeterministic-entry line is the natural bookkeeping); T3's bound
becomes a sum of invariant sizes. The one genuinely new obligation is
the ω-analogue of certification — inclusion of the component's
ω-behavior in the hypothesis's — which is again a component-local
product. Liveness CEGAR with self-naming culprits is the payoff that
justifies the lift.

**Shape choice.** Orthogonal here by fiat; the cost model it should
optimize is visible in T3 and §7 — the sum of contract sizes under the
grouping, and the alphabet each cut exposes.

**Symbolic.** Step 1 is a reachability walk on a product of small
automata; when hypotheses stop being small, that walk is exactly what
hierarchical decision diagrams are for. Nothing above assumed
explicitness except the walk itself.

**Policies.** Remark 4.3's knobs, measured, on a corpus, against the
count differential below.

---

## 9. Oracles first

No stage of an implementation graduates without its oracle named:

- **Verdict parity**: on every model of the corpus small enough,
  Algorithm 4.1 agrees with monolithic explicit-state model checking —
  both verdicts, and the violation words replay concretely.
- **Round assertions**: after every step 3, `Σ|H_i|` strictly
  increased, every `H_i` is certified, and the refuted witness is no
  longer a trace of the abstract product.
- **Budget assertion**: total counterexamples processed never exceed
  `Σ_i (|Q_i| + 1)`; a violation of this bound is a bug in the learner
  or the teacher, and the assertion says which side to suspect.
- **Independent checker**: the certificate checker is a separate
  program, sharing no code with the loop, run on every emitted
  certificate; a certificate that fails its own obligations is a bug
  found before a reviewer finds it.

*Coda.* The loop is three old ideas — CEGAR's refinement discipline,
assume-guarantee's local obligations, MAT learning's class-splitting —
arranged so that each supplies exactly what the others lacked: the
composition names the culprit (so CEGAR loses its heuristic step), the
learner is the refiner (so refinement gains a canonical target and a
budget), and certification is local (so the equivalence oracle loses
its cost). None of the three is new; the claim is that this arrangement
of them is, and T1–T5 are its content.
