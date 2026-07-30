# CEGAR spec — v1: the finite instance, certified and checked

Engineering specification for `cegar_certified_abstractions_v2.md`
(the paper). Scope of v1: **finite LTS leaves, safety, explicit walks,
flat abstract product, shape-descent replay, canonical classifiers,
interning, certificates with an independent checker** — §§1–5 of the
paper plus the oracles of §9. Out of scope, by name: well-structured
leaves (paper §6), interior contracts, discovered alphabets, the ω
lift, symbolic walks (paper §8).

The paper's theorems are the contract; this file restates everything
engineering needs so that no step requires reading the proofs. Where a
choice is free in the paper (policy knobs, Remark 4.4), v1 fixes a
default and makes the knob a CLI flag to be measured, not argued.

## 1. Placement and isolation

New package `include/hsc/cegar/` + `src/cegar/`, docs first (README.md
+ algorithm.md before code). Isolation mirrors `xpl`: the package
depends on `util/` (hashing) only — **no** `core/`, no `lia/`, no
`surface/`. Bridges to the model corpus (DVE processes, NUPN units as
leaves) are a later thread; v1 has its own tiny model format (§5) and
generator, so the loop is measurable end to end without touching the
rest of the system.

Binaries:

* `tools/hsc-cegar.cc` — driver: `run` (the loop), `mono` (monolithic
  oracle walk), `gen` (model families). Links the library.
* `tools/hsc-certcheck.cc` — the independent certificate checker.
  **Shares no code with the loop or the library**: its own parser for
  both file formats, its own two walks, in one self-contained
  translation unit. This duplication is deliberate (paper Remark 5.2,
  §9): the checker is the trusted core, and it must not inherit a bug
  from the code it checks.

Tests: `tests/test_cegar.cc` (doctest, registered in the suite
binary). Experiment records: `experiments/cegar/`.

## 2. Objects

All words are `std::vector<uint16_t>` of letter indices; leaves name
letters `0..|Σ_i|-1`. Sizes are small by design (leaves are meant to
be component-sized); prefer clarity over packing.

**2.1 `lts`** — a leaf: `n_states` (initial is state 0), `n_letters`,
partial `delta` as a flat `n_states × n_letters` vector of `int32_t`
with `-1` = undefined. Operations: `fire(word) → optional<state>`
(membership: defined iff every step defined — paper Remark 1.8);
`serialize()` — a canonical byte string (header + delta table), used
as the interning key (§4.6).

**2.2 `shape`** — a binary tree over leaf indices `0..n-1`; parsed
from an s-expression, default the left comb. Used by replay descent
and by nothing else in v1.

**2.3 `event`** — `id`, support: sorted `vector<pair<leaf, letter>>`,
at most one letter per leaf (separability is the input format's
grammar — paper Def 1.2; no check needed beyond parse).

**2.4 `monitor`** — complete DFA over events: `n_states` (initial 0),
`delta` (`n_states × n_events`), `bad` bitmask. **Format convention:**
a transition not listed in the input file is a self-loop (safety
monitors stutter on irrelevant events); completeness is therefore by
construction.

**2.5 `classifier`** (paper Def 3.1) — the canonical class table:
`k` classes, class 0 = `[ε]`; `reps` (shortlex-least access words, in
shortlex order — so `reps[0] = ε`); right action `act` (`k × n_letters`);
`live` bitmask, `live[0]` true, at most **one** dead class, absorbing.
`L(H) = { u : class(u) live }`; `classify(word) → class` is a fold over
`act`. `serialize()` — canonical bytes; **equality of classifiers is
byte equality of serializations**.

`chaos(n_letters)`: k=1, everything live — the initial rung.

**2.6 `canonicalize(table) → classifier`** — the operation that makes
every rung canonical (paper Def 3.7). Input: any complete class table
with live/dead marking. Steps:

1. merge: all dead classes into one absorbing sink (language-equal by
   prefix-closure — paper Fact 2.2 / Remark 3.2);
2. minimize: Moore partition refinement on (live?, act) — the result
   is the Nerode quotient of `L(table)`;
3. rename: BFS from class of ε in letter order gives the shortlex-least
   access word per class; order classes shortlex by that word, renumber;
   recompute `reps`.

Postconditions (assert in tests): idempotent (canonicalize ∘
canonicalize = canonicalize, byte-identical); two tables with equal
languages canonicalize to byte-equal classifiers.

## 3. Teacher and learner

**3.1 Certification** (paper Def 3.4) — `certify(lts, classifier)`:
BFS on pairs `(q, c)` from `(0, 0)`; for each letter `a` with
`delta[q][a]` defined, step `c' = act[c][a]`; if `c'` dead, return the
access path + `a` as a **positive counterexample** (a word in
`L_i \ L(H_i)`); else continue. No dead pair reachable → certified.
Cost `O(|Q|·k·|Σ|)`.

**3.2 Learner** — L\* with Rivest–Schapire counterexample handling,
one instance per (interned) leaf. Owns an observation table
`(S, E, T)`: `S` prefix-closed access words with pairwise-distinct
rows, `E` suffixes (initially `{ε}`), `T(u·e)` by membership =
`lts.fire`. Closure: while some `s·a` row differs from every `S` row,
promote `s·a` into `S`. Publication: `publish()` builds the table DFA
(classes = `S` rows, action by row lookup, live iff `T(s·ε)`),
**dead-closes** it (transitions out of a rejecting row → sink) and
returns `canonicalize` of the result. Dead-closure may cut real
traces; certification is the judge and feeds back positives — no
other correctness burden here (paper §3.5 discussion).

Counterexample `w` (any word misclassified by the *published*
classifier): Rivest–Schapire binary search over positions `i` of `w`,
comparing `member(rep(class of w[0..i]) · w[i+1..])` at adjacent
split points; the flip yields a distinguishing suffix, added to `E`;
re-close. **Assert (F2): |S| strictly grew.** If the assert ever
fails the bug is in the learner or the teacher, and the budget oracle
(§7, O3) says which side to suspect.

**3.3 Budget accounting** — a per-leaf counter of counterexamples
processed (both sources: replay negatives, certification positives).
Global assert: counter of leaf `i` never exceeds `|Q_i| + 1`
(paper T3 via F1–F3 + Remark 2.3).

## 4. The loop

`cegar::run(model, options) → verdict` implementing paper Alg 4.1.

**4.1 Profile** — per *distinct* leaf (interning, §4.6): learner +
current published classifier + certified flag. Initially `chaos`,
certified for free.

**4.2 Search** — BFS over `(m, c_1..c_n)` (only the ≤n distinct-leaf
classifier tables are consulted; the tuple is over leaf instances).
Event `e` fires: `m' = monitor.delta[m][e]`; each supported
`(i, a)`: `c_i' = act_i[c_i][a]`, blocked if dead. Bad = any reached
state with `m ∈ bad` (check the initial state too). Predecessor map
reconstructs the event word `w`. No bad state → emit certificate
(§6) from the reached set = `Inv`, verdict **holds**. Store: hash map
keyed on the packed tuple; states materialized as reached.

**4.3 Replay** — descent of the shape (paper step 2): at a node with
no support of `w` in its subtree, skip; at a cut, recurse with the
projected words; at leaf `i`, `fire(π_i(w))`. All complete → verdict
**violation**, evidence = `w` + the per-leaf executions. Else
culprits = failing leaves, each with `u_i = π_i(w) ∈ L(H_i) \ L_i`.

**4.4 Refine until resolved** — per culprit (default policy: all
culprits of the witness, in leaf order — knobs in §4.5): repeat
{ feed `u_i` to the learner; `publish`; `certify`; while
certification returns a positive `p`, feed `p`, `publish`, `certify`;
} until certified **and** `u_i` classified dead. Each iteration
processes a misclassified word, so the budget (§3.3) bounds it.

**4.5 Policy knobs** (CLI flags, defaults first): culprit selection
`all|first|cheapest` (default `all`); `--jump-exact` (drive a culprit
straight to its exact rung by iterating certification against the
concrete leaf — off by default). All satisfy T1–T3; they exist to be
measured (table T-D).

**4.6 Interning** — leaves keyed by `lts.serialize()`; equal-key
leaves share one learner, one classifier, one certification, and every
counterexample (paper Remark 5.4). Assert: byte-equal leaves are
pointer-equal profiles at all times (§7, O4). Note the alphabet
bijection of the paper's Remark 5.4 is v1-restricted to the identity
(byte-equal serializations); bijection search is out of scope.

**4.7 Round assertions** (after every refine — §7, O2):
`Σ index(H_i)` strictly increased; every classifier certified and
byte-idempotent under re-canonicalization; every culprit's `u_i` now
dead. Recurrence of a previously refuted witness is legal (paper
Remark 4.2) — no assert may forbid it.

## 5. Model format `.cts`

Line-oriented text, `#` comments, whitespace-tolerant. Grammar:

```
cts 1
leaf NAME states N letters K        # states 0..N-1, initial 0
t SRC LETTER DST                    # within the current leaf; omitted = undefined
shape SEXPR                         # optional; leaf names; default left comb
event NAME LEAF:LETTER [LEAF:LETTER ...]
monitor states M                    # initial 0
m SRC EVENT DST                     # omitted (SRC,EVENT) = self-loop
bad STATE [STATE ...]
```

`LETTER` numeric; `LEAF`/`EVENT` by name. The parser rejects:
duplicate `t` for one `(SRC,LETTER)` (determinism), a leaf twice in
one event's support, out-of-range indices.

## 6. Certificate format `.cert` and the checker

```
cert 1
classifier LEAFNAME classes K dead D    # D = dead class index, or -1
a CLASS LETTER CLASS                    # the full action table
inv M C1 ... Cn                         # one line per Inv state, leaf order
```

One `classifier` block per *distinct* leaf; instance leaves reference
their class's table (the emitter writes a `use LEAFNAME LEAFNAME`
line aliasing instance → representative). Reps are not written:
obligations never read them (canonicity is the loop's discipline, not
a soundness obligation).

`hsc-certcheck model.cts proof.cert` checks, and reports pass/fail
per obligation (paper Def 5.1, T5):

* **(L_i)** per distinct leaf: product walk lts × table reaches no
  dead class;
* **(G1)** the initial abstract state is in `Inv`;
* **(G2)** `Inv` closed under every event (blocked-by-dead counts as
  closed);
* **(G3)** no `Inv` state has a bad monitor coordinate.

Exit 0 iff all pass. Mutation discipline (§7, O5): the test suite
corrupts emitted certificates (drop an `inv` line; flip a `live`/dead;
redirect one action entry; enlarge `bad`) and requires exit ≠ 0.

## 7. Oracles (paper §9 — no milestone graduates without its oracle)

* **O1 verdict parity**: `run` and `mono` agree on every corpus model;
  every violation word replays concretely (`mono` re-fires it).
* **O2 round assertions**: §4.7, compiled in for tests and sweeps.
* **O3 budget**: §3.3, total counterexamples ≤ `Σ (|Q_i|+1)` —
  counted over *distinct* leaves (interning shares the budget).
* **O4 interning**: byte-equal leaves ⇒ pointer-equal profiles.
* **O5 independent checker**: every certificate emitted in any test or
  sweep passes `hsc-certcheck`; mutated certificates fail it.

## 8. Milestones

Each lands with its tests green and its oracle named in the test file;
report progress in `cegar_report.md` as milestones close.

* **M0 model kit.** Package docs; `.cts` parser + writer; `mono`
  (monolithic BFS on `(m, q_1..q_n)`, direct product — paper Prop 1.3);
  `gen` families (§9). Oracle: the paper's §6 worked example in
  `tests/` as a `.cts` literal — `mono` says holds; its bugged variant
  — `mono` produces a violation that re-fires.
* **M1 classifier.** Table, `canonicalize`, serialize, `chaos`.
  Oracle: idempotence; language-equality ⇒ byte-equality on
  hand-built shuffled tables; `classify` agrees with table walks.
* **M2 teacher.** `certify`. Oracle: `certify(lts, chaos)` passes for
  every leaf; `certify` against a hand-written too-strict table
  returns a word that is a trace (`fire` accepts) and is dead in the
  table.
* **M3 learner.** L\*+RS against the leaf as teacher, certification as
  the equivalence oracle (loop: certify → feed positive → repeat;
  plus, for the test only, the reverse inclusion check
  table × complement-walk to drive to exactness). Oracle: on every
  generated leaf, converges to `index ≤ |Q|+1` with language equality
  vs the leaf (checked by product walks both directions); F2 asserted
  at every counterexample.
* **M4 loop.** `run` = search + replay + refine; verdicts + stats
  (rounds, counterexamples, rung profile, abstract states walked).
  Oracle: O1 on the full corpus, O2/O3 compiled in. Expected on
  `clients(k)`: exactly the paper's §6 phenomenon — server driven
  exact, clients chaotic, `Inv` of 3 abstract states at k=2.
* **M5 certificates.** Emitter + `hsc-certcheck`. Oracle: O5.
* **M6 interning + policies + campaign.** §4.6, §4.5 flags, sweep
  scripts; tables T-A..T-D into `experiments/cegar/` and the report.
  Oracle: O4; sweeps run under O1–O3.

## 9. Generator families (`gen`)

Deterministic in a `--seed`; every family prints its exact parameters
into a `#` header of the emitted `.cts` (reproducibility: the file is
the record).

* `clients K` — paper §6 generalized: one server (2K+1 states: free +
  busy_i), K interned-identical clients. Property: no grant while
  outstanding. **Holds.** Expected: server → exact (index 2K+2 wait —
  measure it), clients stay chaotic; refinements all land on the
  server (paper §6 prediction).
* `clients-bug K` — server forgets to block on client 1 vs 2 overlap.
  **Violates**; witness of length 2.
* `ring N` — token ring, N interned-identical stations, mutual
  exclusion monitor. **Holds.** Symmetric: one learner for all
  stations (O4 stress).
* `rand L Q S E D SEED` — L leaves of ≤Q states over ≤S letters
  (random partial deterministic), E events with random supports of
  density D, random monitor with random bad set. Both verdicts occur;
  the parity workhorse for O1. Trim: reject instances whose `mono`
  walk exceeds 10^6 states (keeps every diagnostic under the 15 s
  cap).

## 10. Experiments (tables for the paper; records under `experiments/cegar/`)

All sweeps: TSV, one row per (model, config, seed), columns fixed in
the sweep script header; scripts + TSVs committed; 15 s per-model cap,
a timeout is a row with `status=timeout`, never a discard.

* **T-A parity.** Corpus = all families + `rand` grid (≥200 models).
  Columns: model, verdict_cegar, verdict_mono, agree, witness_len,
  witness_replays. Expected: 100 % agree — this is O1 as data.
* **T-B budget & cone.** Same corpus, holds-instances. Columns:
  Σ(|Q_i|+1) (distinct leaves), counterexamples spent, leaves exact /
  intermediate / chaotic, |Inv|, abstract states walked, mono states
  walked. Claims priced: spent ≤ budget always; chaotic count = the
  emergent cone (paper Remark 4.3).
* **T-C symmetry scaling.** `clients(K)`, `ring(N)`, K,N ∈
  {2,4,8,16,32,64}: refinements, |Inv|, walltime for `run` vs `mono`,
  interning on vs off (flag for the ablation). Expected: `run` flat-ish
  in K where `mono` grows; interning-off multiplies refinements by
  ~K but not the verdict (T1–T3 are policy-independent).
* **T-D policy knobs.** `rand` corpus × {all, first, cheapest} ×
  {jump-exact on/off}: rounds, counterexamples, abstract states,
  walltime. No prediction — this is the measurement the paper's
  Remark 4.4 promises.

## 11. Reproducibility constraints

Everything in the report traces to a committed TSV under
`experiments/cegar/` produced by a committed script; seeds and
parameters live in the TSV or the `.cts` headers; the certificate of
every `holds` row in T-A is re-checked by `hsc-certcheck` inside the
sweep (a column, not a claim).
