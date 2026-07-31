# CEGAR spec — v2: the finite instance, HSC-native

Engineering specification for `cegar_certified_abstractions_v3.md`
(the paper). Scope: **finite LTS leaves, safety, explicit walks, flat
abstract product, replay by projection, canonical classifiers,
interning, certificates with an independently checkable proof** —
§§1–5 of the paper plus the oracles of §7 below. Out of scope, by
name: well-structured leaves (paper §7), interior contracts,
discovered alphabets, the ω lift, symbolic walks (paper §10).

There is **one language and one parser** in this repository: `.hsc`,
read by the surface parser. The loop's input is an `.hsc` model; the
loop's commands are commands of the main (`hsc`); the certificate is
an `.hsc` document. No side formats, no side binaries.

The paper's theorems are the contract; this file restates everything
engineering needs so that no step requires reading the proofs. Where
a choice is free in the paper (policy knobs, Remark 4.5), v2 fixes a
default and makes the knob a command option to be measured, not
argued.

## 1. Placement

* `include/hsc/cegar/` + `src/cegar/` (static lib `hsc_cegar`) — the
  calculus core: `classifier`, `learner`, `certify`, `loop`, and the
  POD `model` (leaves as transition tables, events as per-leaf
  letters, monitor, verdicts, `mono`, `refire`). **Parser-free**: the
  core neither reads nor writes text except the certificate emitter
  (sexpr text out, §6). It depends on `util/` only.
* `src/surface_cegar.cc` (+ `include/hsc/surface/cegar_build.hh`) —
  the bridge: builds a `cegar::model` from the parsed spec (§5), maps
  verdicts back to session results, parses certificate documents.
* `src/surface_certcheck.cc` — the checker command's implementation:
  its own two walks and closure scan in one small translation unit.
  It shares the parser and the §5 builder with everything else — one
  grammar, one parser — and **none of the loop's logic**: no search,
  no learner, no canonicalizer. A bug in the loop cannot certify
  itself.
* Commands (§4.8) live in the main's runner (`surface_run.cc`
  routing). **No cegar binaries.**

Tests: `tests/cegar/` (own doctest binary; links `hsc_cegar` and the
surface library — fixtures are `.hsc` literals; random-model parity
fuzzing lives here, in-process, and nowhere else). Model families:
`examples/cegar/*.hsc` (parametric). Experiment records:
`experiments/cegar/`.

## 2. Objects (the core's PODs)

All leaf words are `std::vector<uint16_t>` of letter indices; event
words are event-index vectors. Sizes are small by design; prefer
clarity over packing.

**2.1 `lts`** — a leaf: `n_states` (initial is state 0), `n_letters`,
partial `delta` (`n_states × n_letters`, `-1` = undefined).
`fire(word) → optional<state>` (membership — paper Remark 1.8);
`serialize()` — canonical bytes, the interning key. The bridge
supplies, beside each leaf, its `value_of` table (state → the leaf's
`.hsc` value; state numbering is fixed by §5.4, so byte-equal leaves
have identical tables).

**2.2 `event`** — name + sorted support `(leaf, letter)*`, at most
one letter per leaf.

**2.3 `monitor`** — complete DFA over events with a `bad` mask. Not
an input object: the bridge *derives* it from the property atoms
(§5.6). `n_states × n_events` delta, complete by construction.

**2.4 `shape`** — v2 uses the frontier order only: replay projects
per leaf and skipping is "no support" (paper Prop 1.6 collapses to
the leaf form, Prop 1.5, over a flat frontier; the tree adds nothing
to the finite instance's replay beyond skip-wholesale, which the
support sets already give). The declared `.hsc` shape governs the
symbolic engines and is not consulted here.

**2.5 `classifier`** (paper Def 3.1) — canonical class table:
class 0 = `[ε]`, shortlex reps, right action, live mask, at most one
absorbing dead class. Byte serialization; **equality is byte
equality**. `chaos(n_letters)` = the one-class table.

**2.6 `canonicalize(table)`** — merge dead (sound by prefix-closure,
paper Fact 2.2/Remark 3.2), Moore-minimize, shortlex-rename.
Idempotent; language equality ⇒ byte equality (asserted in tests).

## 3. Teacher and learner

**3.1 Certification** (paper Def 3.4) — `certify(lts, classifier)`:
BFS on pairs from `(0,0)`; a reachable `(q, dead)` returns its access
word, a **positive counterexample** in `L_i \ L(H_i)`; none →
certified. Cost `O(|Q|·k·|Σ|)`.

**3.2 Learner and the publication discipline (F1).** L\* with
Rivest–Schapire counterexample handling, one instance per interned
leaf. The learner's observation table is **internal state and never
an abstraction**: a closed table already rejects words the leaf can
fire, so consuming it uncertified is unsound in the dangerous
direction (paper Remark 3.7). The published classifier — the only
object the loop may read — is:

* `chaos`, from creation until the learner processes its **first**
  counterexample (the table exists, closes, and is *not* published);
* thereafter `canonicalize(dead_closure(table DFA))`, and **only
  after** the certification of §4.4 has passed on it.

The loop's invariant: *every classifier consulted by search, replay
or certificate emission is certified* — chaos for free, every later
publication behind a completed `certify`. There is no code path that
hands the raw table DFA to the search. (Enforced by construction:
the profile stores only published classifiers; `publish()` is called
inside refine, §4.4, never by the search.)

Counterexample processing: Rivest–Schapire binary search for the
distinguishing suffix, add to `E`, re-close. **Assert (F2): |S|
strictly grew.**

**3.3 Budget** — per-interned-leaf counter of counterexamples (both
sources). Assert: never exceeds `|Q_i| + 1` (paper T3 via F1–F3 and
Remark 2.3).

## 4. The loop and its commands

`cegar::run(model, options) → run_result` implementing paper
Alg 4.1. §§4.1–4.7 are unchanged in substance from v1 of this spec
and remain the implementation's shape:

**4.1 Profile** — per distinct leaf (interning by `lts.serialize()`):
learner + published classifier + certified flag; initially chaos.

**4.2 Search** — BFS over `(m, c_1..c_k)` where the classifier
coordinates range over the **non-property** leaves (property leaves
are tracked exactly by the monitor, §5.6, and carry no classifier
coordinate); an event fires through the monitor delta (blocked where
the monitor blocks) and each supported leaf's right action, blocked
when a supported class goes dead; bad = monitor bad mask. No bad
reachable → certificate from the reached set (§6), verdict
**holds**.

**4.3 Replay** — for each non-property leaf with support in the
witness, fire the projection (skip the rest wholesale; property-leaf
projections are fired too, as an assertion — the monitor's exactness
makes their failure a bridge bug, not a culprit). All complete →
verdict **violation** (validated by the executions; the final
concrete state is assembled from the leaf executions' end states and
bound as the result, §4.8). Else culprits = the failing non-property
leaves.

**4.4 Refine until resolved** — per culprit (policy knob): feed the
negative counterexample; `publish`; `certify`; feed back positives
until certified **and** the culprit word classifies dead.

**4.5 Policy knobs** — culprit selection `all|first|cheapest`
(default `all`); `jump-exact` (default off). All satisfy T1–T3.

**4.6 Interning** — byte-equal leaves share one profile entry, one
budget. Assert pointer-equality of profiles for byte-equal leaves.

**4.7 Round assertions** — after every refine: `Σ index` grew; every
classifier certified and idempotent under re-canonicalization; every
culprit word now dead. Witness recurrence across rounds stays legal.

**4.8 Commands** (the main's runner; grammar in the manual §8):

```lisp
(cegar NAME QATOM+ [all|first|cheapest] [jump-exact] [no-intern] [cap INT])
(certificate FILE)
(certcheck FILE)
```

The runner auto-appends `simplify-constants`, `simplify-arrays`,
`flatten` to the rewrite chain of any file containing a `cegar`
command (unless already present; the trace lines show it), so every
engine in the file sees the same normalized spec.

* `cegar` builds the model (§5) and monitor (§5.6) from the current
  spec and the atoms, runs the loop, prints one summary line
  (verdict, rounds, counterexamples/budget, rung counts, |Inv|,
  abstract states) plus, on violation, the event-name word. It binds
  `NAME` as an explicit result: the validated bad state (one word)
  on violation, the **empty** result on holds — so `(expect NAME 0)`
  asserts "holds" in scripts, and `get-witness`/`get-states`/`count`
  work unchanged. On holds the session retains the certificate text.
* `certificate FILE` writes the retained certificate (§6); when the
  last `cegar` did not hold it prints a skip note and continues —
  linear driver scripts run unchanged on both verdicts.
* `certcheck FILE` checks a certificate document against the current
  spec (§6); per-obligation pass/fail lines, nonzero exit on any
  fail.
* Loop meters (rounds, counterexamples vs budget, rung profile, time
  split: leaf oracles / search / replay) are also printed by
  `(bill)`.

## 5. Input: the separable fragment of `.hsc`

The bridge consumes the session's parsed spec (`surface::spec`): leaf
order, declared bounds, seeds, and each event's raw clauses. It
**accepts** exactly the separable fragment and **refuses loudly** —
naming the event and the construct — everything else. Refusals are
the paper's deferred relaxations, not errors to work around.

**5.1 Leaves.** Every leaf must carry a declared finite bound
`[lo, hi)`. Unbounded leaf → refuse (finite instance).

**5.2 Seeds.** Exactly one initial state (the base word, possibly
pair-edited). `alt`/`havoc` init → refuse.

**5.3 Events.** Plain `(event NAME (when …) (do …)+)` only; `alt`,
`seq`, `havoc`, and dynamic array indexes (`(at A E)` with non-const
`E`) → refuse. Separability checks, per event: every `when` atom's
`lia` support (`support_bool`) lies within **one** leaf; every action
writes one scalar cell whose `rhs` support lies within **that same**
cell. The event's support = the union of touched leaves.

**5.4 Induced leaf LTS.** For leaf `p` with bound `[lo,hi)` and seed
value `s`: states are the domain values renumbered so the seed is
state 0 (the permutation `s, lo, …, ŝ, …, hi-1` — deterministic, so
byte-equal leaves stay byte-equal); `value_of` records the inverse.
For each event supporting `p`, its **local action** is computed by
enumeration over the domain: guard atoms on `p` evaluated with `p=v`
(sound: their support is `{p}`), then the `do` steps folded
left-to-right. Guard false at `v` → undefined at `v`. A write landing
outside `[lo,hi)` on an enabled guard → **refuse the model** (loud;
matches the explicit engine's overflow-is-error stance, keeps parity
meaningful).

**5.5 Letters.** A leaf's letters are the *distinct* local-action
graphs over its domain, sorted lexicographically — the numbering is
canonical, so interning and classifier bytes are stable across
instances. Each event maps, per supported leaf, to its letter index.
Two events inducing the same local action share the letter (the
calculus's letters, not the events, label leaf transitions).

**5.6 Property and monitor.** `(cegar NAME QATOM+)` atoms are read
by the expression reader; each atom's support must lie within one
leaf (crossing atoms → refuse; they are the non-separable property
case). The named leaves are the **property leaves**. The monitor is
their exact sub-product: states = tuples of property-leaf values
reachable from the seed tuple (bounded by `Π` their domains), delta
per event = the local actions where defined, **blocked** (`-1`)
where an action is undefined at the tuple — exactly the leaves' own
blocking, tracked at full precision. `bad` = tuples satisfying every
atom. Conceptually this is the paper's Def 1.7 DFA composed with the
property leaves at their exact rung from round zero (a legal
jump-to-exact, T3-neutral since they never spend afterwards).
Consequences, all load-bearing: property leaves carry **no
classifier coordinate** (§4.2), are **never culprits** (a witness's
property-leaf projections always fire — asserted at replay), are
**excluded from interning** (a byte-equal non-property sibling
interns separately, so refinement of the sibling never advances a
property leaf's rung), and spend **no budget**.

**5.7 The builder's oracle (O6).** On every corpus model,
`cegar::mono` (the core's concrete product walk) and `(xreach)` on
the same spec agree on the reachable-state count and on
bad-emptiness. This pins the induced semantics of §5.4 to the
engine's; a disagreement is a builder bug, never a corpus artifact.

## 6. The certificate document and the checker

An `.hsc` s-expression document (the one parser), vocabulary known to
the main:

```lisp
(certificate (select QATOM+))        ; header: the property, echoed
(classifier LEAF CLASSES DEAD        ; one per distinct non-property leaf;
  (a CLASS LETTER CLASS)*)           ;   DEAD = -1 if none; full action table
(use LEAF REPLEAF)                   ; instance → representative alias
(inv (LEAF X)*)                      ; one per Inv state: X = value for
                                     ; property leaves (they determine the
                                     ; monitor state), class index otherwise
```

Letters are the canonical indices of §5.5 — the checker recomputes
them from the current spec, so the document carries no letter tables.
Reps are not written: canonicity is the loop's discipline, not a
soundness obligation.

`(certcheck FILE)` under the current spec rebuilds the induced leaves
(§5) and the monitor from the echoed atoms, then checks (paper
Def 5.1, T5):

* **(L_i)** per distinct leaf: `lts × table` product walk reaches no
  dead class;
* **(G1)** the initial abstract state is an `inv` entry;
* **(G2)** the `inv` set is closed under every event (blocked-by-dead
  counts as closed);
* **(G3)** no `inv` entry satisfies the property atoms.

Pass/fail per obligation; exit nonzero on any fail. Mutation
discipline (O5): tests corrupt emitted certificates (drop an `inv`,
flip a dead index, redirect an action, weaken the select) and require
failure.

## 7. Oracles (no milestone graduates without its oracle)

* **O1 parity**: `run` and `mono` agree on every corpus model; every
  violation refires concretely.
* **O2 round assertions**: §4.7, compiled into tests and sweeps.
* **O3 budget**: total counterexamples ≤ `Σ (|Q_i|+1)` over distinct
  leaves.
* **O4 interning**: byte-equal leaves ⇒ pointer-equal profiles.
* **O5 checker**: every emitted certificate passes `certcheck`;
  mutated ones fail.
* **O6 builder**: §5.7 — `mono` vs `xreach` on every corpus model.

## 8. Milestones

* **P0 docs.** This spec; `cegar/README.md` and `algorithm.md`
  reflect the HSC-native design; manual §8 gains the three commands.
* **P1 builder.** §5 bridge + O6 green on the §9 families.
* **P2 commands.** `cegar` + `certificate` wired into the runner;
  O1–O4 green; the §9 examples self-check (`expect` lines).
* **P3 checker.** `certcheck` + O5, mutation tests.
* **P4 purge.** No trace of the retired side formats anywhere in the
  tree (grep for their extensions comes back empty outside `.git`);
  old binaries and generator deleted; tests are `.hsc`-fixture-based.
* **P5 campaign.** Sweeps re-run end to end from `.hsc` corpora;
  tables T-A..T-D regenerated; report updated.

## 9. Model families

Parametric `.hsc` in `examples/cegar/`, each self-checking:

* `clients.hsc` — `(param K …)`: one server (free + busy_i), K
  identical clients, monitor leaf `mon` tracking
  quiet/outstanding/double-grant; property `(== mon 2)`. **Holds.**
  Expected: server exact, clients chaotic (paper §6).
* `clients_bug.hsc` — the server forgets the busy check.
  **Violates**; witness length 2.
* `ring.hsc` — `(param N …)`: token ring of identical stations,
  mutual-exclusion monitor. **Holds.** One interned entry for all
  stations (O4 stress).
There is **no random corpus in the campaign**: random models are a
correctness fuzzer, not a measurement — that role lives in the test
suite (parity vs `mono`, all policies, pinned regression seeds) and
its rows belong in no table. Known gap, reported not hidden: the
bridge's letter induction is exercised by the families and the unit
fixtures, not fuzzed. Measurement corpora beyond the families are the
real ones (DVE, NUPN), queued behind the fragment-coverage question
(§10 of the paper).

## 10. Experiments (tables for the paper; records in `experiments/cegar/`)

Family-based; measurement on random soup is not evidence (§9). All
sweeps: TSV, one row per (model, config), scripts + TSVs committed,
15 s per-run cap, a timeout is a row. Per-row parity triangle:
the `cegar` verdict against the symbolic engine (`reach` + `select`
of the bad atoms), `xreach` for concrete counts, `certcheck` on
every holds — three independent implementations agreeing, as data.

* **T-A parity & budget**: clients, clients-bug, ring across sizes;
  verdicts, agreement, certcheck, cex vs pool, rung profile, |Inv|,
  abstract vs concrete states.
* **T-C symmetry scaling**: clients(K), ring(N), K,N ∈
  {2,4,8,16,32,64}; interning on/off ablation; the time split.
* **T-D policy knobs**: deferred until the refinement-heavy corpus
  exists (the queued theory item) — the families resolve in too few
  rounds for the policies to diverge, and measuring that would be
  noise dressed as data.

New column set, all tables: the time split (leaf-oracle time,
search time, replay time) — the teacher-locality measurement.

## 11. Reproducibility

Everything in the report traces to a committed TSV under
`experiments/cegar/` produced by a committed script; parameters live
in the TSVs and the generated `.hsc` headers; the certificate of
every `holds` row in T-A is re-checked by `certcheck` inside the
sweep (a column, not a claim).
