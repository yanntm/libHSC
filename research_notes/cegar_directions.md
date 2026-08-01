# CEGAR — directions

Prospective material for `hsc_cegar.md`: what the paper deliberately
excludes, kept here so the paper stays clean. Each entry names the
opening, what is already known, and the question that would make it a
contribution. Full earlier treatments live in git history
(`cegar_certified_abstractions_v3.md`, §§7, 9, 10).

## 1. Interior contracts

A classifier at any node `ν` over its visible alphabet `A_ν`,
certified against `L_ν` by a subtree-local product; replay's descent
then stops at the highest certified node that rejects, and (L)
obligations attach to interior nodes of the certificate tree. Known:
Definition 2.4 of the paper is stated at every node precisely so this
extension changes the *placement* of hypotheses and no definition.
The question: the size of `L_ν`'s canonical classifier under hiding —
the projection exposure, to be priced before paid or not at all.

## 2. Discovered alphabets — dropping separability

The query/case bracket compiles a non-separable criterion into
finitely many `(event, residual)` pairs, discovering an exchange
alphabet mid-run. The cut decomposition survives over
residual-extended alphabets, but alphabets grow while learners run:
hypotheses must default unseen letters to chaos, and certification
must be re-established on growth. MAT learning tolerates alphabet
extension; the bookkeeping is real and wants its own draft.

## 3. The ω lift

Classifier → syntactic invariant of an ω-language (the syntactic
congruence [Arn85, MS97]); word executions → lasso evaluations; the
walk threads acceptance. The loop consumes classifiers through three
verbs only — membership, right action, certification — so the lift
swaps the §3 notion, not the loop. The new obligation is
ω-certification (inclusion of the leaf's ω-behavior in the
hypothesis's), again local. Pieces on the shelf: lasso
representations [CNP93], learnability of infinitary sets [MP95],
active learners over families of DFAs [AF16, LCZL17] and
deterministic ω-automata [BL21]. Keys resolve in
`cegar_citations.md`.

## 4. Well-structured leaves

The teacher contract (paper Definition 3.5) never mentioned
finiteness. For a leaf whose behavior is the trace language of a WSTS
(wqo, effective pred-basis), membership is still one execution and
certification is coverability of `{(m, d) : d dead}` — decidable by
backward saturation [AČJT96, AČJT00, FS01]; [AČJT96] states the
needed instance verbatim. T1, T2, T4 hold as proved (they used the
contract, not the walks); T3 fails and must (`N(L_i)` infinite for
one unbounded counter). Known result, proved in the v3 text: with a
persistent per-leaf negative store (prefix-tree quotient, met into
the published classifier) and length-fair search, the loop is a sound
semi-algorithm complete for violations. Pleasant specialization: for
a single unbounded counter the rungs are exactly the classical
counter abstractions, refined on demand, certified by coverability.
Making this a focus is a different paper.

## 5. Symbolic walks

Step 1 is reachability over a product of small tables; when indices
grow, that walk is what the symbolic engines are for. Nothing in the
paper assumed explicitness except the walk itself — the plug point is
clean. This is also where the loop and the house baseline stop being
comparators and compose.

## 6. Evaluation program

What an experimental section must establish, when it is built —
against the in-repo explicit and symbolic engines on the shared
corpora (families, DVE, MCC), libDDD/ITS-tools as the external
baseline, bounded-or-skipped:

- **Correctness discipline**: verdict parity with the monolithic
  engines wherever they fit in budget; every violation replayed
  concretely; every certificate independently checked. An oracle the
  campaign runs under, not a benchmark.
- **The cone emerges**: on properties touching a bounded region of a
  large model, untouched leaves stay chaotic and the abstract product
  stays near the monitor's size while the concrete product grows.
  The global-enforcement pole (paper §5) reported honestly as the
  losing case, certificate as the compensating deliverable.
- **Symmetry leverage**: refinement cost per behavior, not per
  instance; whether real corpora expose enough byte-equal leaves is
  itself a finding about the front ends' currification quality.
- **Budget adherence**: counterexamples spent vs the T3 bound,
  distributions not maxima.
- **Certificate economics**: check time vs verification time;
  incremental re-check after a leaf edit vs re-verification.

Positioning gate, before the contribution claim is final: acquire and
read AGAR (Gheorghiu Bobaru–Giannakopoulou–Păsăreanu, CAV 2008) — the
nearest published cousin; see `cegar_citations.md`.

## 7. The CAC08 re-encoding experiment

The corpus of [CAC08] is translated and in the tree
(`examples/cac08/`, converter `tools/fsp2hsc`). Their result: under
two-way decomposition, assume-guarantee rarely beats the monolith —
and our first sweeps reproduce their verdicts. The open experiment:
find a *re-encoding* of these models — a decomposition grain, e.g.
semantic factoring of the store components into counters/per-item
bits — under which decomposed verification actually wins. Finding one
is a result; not finding one confirms [CAC08] from a new angle and is
recorded in the corpus README, not here. A naive per-state one-hot
re-encoding is known insufficient: the one-hot mutex is itself a
cross-leaf correlation and its ghosts can dominate.

## 8. Refinement-heavy corpora

The policy knobs (culprit selection, jump-to-exact) are
measurement-not-proof by paper Remark 4.3, but the synthetic families
resolve in too few rounds for policies to diverge. Measuring them
needs generators with controlled spurious-chain depth — a
corpus-design question, queued with the evaluation.
