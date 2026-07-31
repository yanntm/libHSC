# fsp — grammar, grounding, and the mapping to the separable fragment

## 1. T2M: the language (the CAC08 subset of FSP)

```
file      ::= item*
item      ::= 'const' NAME '=' expr
            | 'range' NAME '=' expr '..' expr
            | 'minimal'? 'property'? procdef
procdef   ::= NAME '=' init (',' statedef)* ext* '.'
init      ::= stateref | body            ; inline body = one anonymous state
statedef  ::= NAME ('[' NAME ':' rng ']')* '=' body
body      ::= '(' branch ('|' branch)* ')' ext* | 'STOP'
rng       ::= NAME | expr '..' expr      ; inclusive bounds, both ends
branch    ::= ('when' '(' bexpr ')')? label ('->' label)* '->' target
target    ::= NAME ('[' expr ']')* | 'ERROR' | stateref-with-args
stateref  ::= NAME ('[' expr ']')*
ext       ::= '+' '{' lpat (',' lpat)* '}'   ; alphabet extension
            | '\' '{' lpat (',' lpat)* '}'   ; hiding
label     ::= lelem (('.')? lelem)*
lelem     ::= NAME
            | '{' lpat (',' lpat)* '}'       ; set: choice over patterns
            | '[' NAME ':' rng ']'           ; binder: choice, name bound
            | '[' rng ']'                    ; anonymous choice
            | '[' expr ']'                   ; computed index
lpat      ::= lelem+                         ; a label pattern (no binders)
```

Expression precedence, loosest first: `|| &&` · `== !=` · `< <= > >=` ·
`<< >>` · `+ -` · `* / %` · prefix `! -` · atoms (INT, NAME, parens).
Comments `//` and `/* */`. A `[' expr ']` whose expression is a lone
range name is an anonymous choice, not an index — ranges and constants
share no namespace in the corpus.

`STOP` is a state with no branches (a deadlocked state; in a property,
totalization sends its whole alphabet to ERROR). Refused at parse: `||`
composite processes, parameterized process headers `P(X = v)`,
relabelling `/{…}`, `progress`, `assert`, LTL, `>>`-priority — the
authors' unsized template files (`peterson.lts`) use these; the sized
per-subject files never do. `minimal` is parsed and
ignored: it is a state-count optimization of LTSA's (weak-bisimulation
quotient after hiding); dropping it preserves every trace and so the
safety verdict — our tau steps stay explicit local events.

## 2. M2LTS: grounding one process

Each process is compiled to a ground LTS exactly as LTSA would:

* A **ground state** is (statedef, argument tuple), arguments within the
  declared inclusive ranges. A *target* whose arguments fall outside is
  **ERROR** — LTSA's convention, and load-bearing: `never_fill_table`
  errs precisely by walking its counter off its range. A target naming
  the process itself refers back to its initial state. Prefix chains
  `a -> b -> S` introduce a fresh anonymous state per `->` under each
  valuation. States are numbered in BFS discovery order from the initial
  reference; **the initial state is 0** (so the `.hsc` base word, every
  leaf at its LO, is the composed initial state, and `(init)` needs no
  edits).
* A **ground label** is a sequence of atoms (names and integers); `[i]`
  indexing and `.` both concatenate. Expanding a label pattern under the
  environment (constants + state parameters + label binders bound so
  far, left to right) yields the choice set: sets and anonymous ranges
  choose independently per element, a binder `[v:rng]` chooses and binds
  `v` for the rest of the branch (later labels, target arguments).
* A `when` guard is evaluated under constants + state parameters; false
  kills the branch at that valuation.
* `ERROR` is a designated sink state (kept only by the property; a
  non-property process targeting ERROR is refused).
* The **alphabet** is the set of ground labels appearing on transitions,
  union the grounded `+{…}` extension patterns.
* **Hiding** `\{…}` matches ground labels by atom-sequence prefix; a
  hidden label's transitions become **tau** (process-local) edges and
  leave the alphabet. Hiding is per process definition, so two processes
  hiding the same name never synchronize on it.

## 3. M2M: composition → the separable fragment

One leaf per non-property process, in file order, domain `[0, n)`
(`n` = ground states), seed 0. The property becomes the **monitor leaf**
`mon`, last in the spine, domain `[0, n+1)` with `E = n` the error value.

**Per-label events.** For each ground label `a` in the union of
alphabets, the participants are the processes (and the monitor) whose
alphabet contains `a`. FSP synchronization: `a` fires iff every
participant takes an `a`-transition. Each participant's `a`-relation is
partitioned into **target pieces**:

* one piece per distinct target `t` with sources `{s | s -a-> t, s ≠ t}`
  — guard `(in P s…)`, action `(:= P t)`;
* one **identity piece** gathering the self-loops `{s | s -a-> s}` —
  guard only, no write (guard-only participation is separable; the
  leaf is in the event's support through its guard).

One event is emitted per element of the product of the participants'
piece sets — this handles nondeterminism (a source with two targets
sits in two pieces) with no product over sources. Pruned: the
all-identity tuple (a pure stutter, no successor change), and any label
some participant has in alphabet but never fires (a globally dead
label — counted and reported, not emitted). Guards, one atom per leaf;
writes, one constant per cell: every event is separable by
construction (spec §5.3).

**Tau events.** A process's tau edges compose with nobody: per target
piece, an event guarding and writing that leaf alone. A tau self-loop
is a stutter and is dropped.

**The monitor** is the property DFA totalized over its own alphabet:
per label, the normal target pieces, plus an **error piece** — sources =
states with no `a`-transition — writing `E`; `E` itself is absorbing
(it joins each label's identity piece). A nondeterministic property is
refused. Labels outside the property's alphabet never touch `mon`
(the property observes a sub-alphabet; e.g. the Gas Station mutual
exclusion property watches pump 1 only).

**Naming.** Leaf = process name minus its `TASK_` prefix; monitor =
`mon`. Event = the ground label's atoms joined by `_`, suffixed `__k`
when a label yields several events; tau events `P__tau_k`. Collisions
uniquified by suffix.

## 4. What the emitted files look like

The **model** file: a provenance header (source `.lts`, the paper's S1
decomposition if a companion `.txt` was given), per-leaf state maps as
comments (`; P: 0=START 1=PREPAYING.1.0 …`), the `leaf`/`shape`/`init`
forms, then the events. The **driver** file: `(input MODEL)`,
`(cegar v (== mon E))`, `(xreach x)` — verdict and count expectations
are pinned by the campaign after the first verified run, not guessed by
the translator.

**Cost note.** Events per label = product over participants of
(#distinct non-self targets + 1); in the parameterized systems this is
small. Chiron's pre-flattened dispatcher (42 071 states at 5 artists)
makes it Σ ≈ the dispatcher's transition count — large but linear; if
that bites, the relief is affine-piece detection (counters emitting
`(+= P k)` for whole source classes), deferred until measured.

## 5. Factoring the stores (`--factor`)

CAC08's negative result is structural: every system centers on a
*store* process (operator counters, table bitmask, dispatcher
registration list) and any cut at process boundaries crosses it at
full width — the assumption must carry the whole store. The corpus
itself shows the fix: chiron *multiple* is the authors' hand-factored
dispatcher, and it is where abstraction leverage appears. `--factor`
mechanizes that re-decomposition at the ground-LTS level, before
emission.

**Coordinates.** Each ground state carries factoring metadata from the
grounder: a **phase** (the statedef, or the anonymous chain position)
and a **register file** — one register per state-parameter or label
binder name, holding its value where in scope and a canonical 0 where
not (sound: an out-of-scope register is never read before rewritten).
The candidate decomposition of a leaf is one coordinate per register
with more than one observed value, plus the phase.

**Validation, per label.** The label's relation `R` factors over
coordinates iff it equals the synchronized product of its per-
coordinate projections `R_c` on reachable states: `s ∈ dom(R)` exactly
when every `s_c ∈ dom(R_c)`, and `R(s) = Π R_c(s_c)` as sets (guards
are rectangles, actions act coordinatewise; per-coordinate
nondeterminism is allowed, cross-coordinate correlation is not).
A label that fails **merges every coordinate it touches** (writes, or
guards — `dom(R_c)` proper) into one, and validation restarts; the
fixpoint is the finest factoring consistent with the dynamics. This
is the heuristic search: coordinates are proposed by names, disposed
by semantics.

**Emission.** Each surviving coordinate becomes a leaf (`P_ac1`, …)
holding that quotient LTS; a label enters a coordinate's alphabet only
where its action there is non-trivial (moves it, or restricts its
domain) — total-identity coordinates do not participate, so factoring
*narrows* event supports. Hidden labels factor like any other and stay
process-local. §3's composition then applies unchanged. Per-target
pieces collapse where the factored action is uniform (table `put[k]`
becomes one `(:= P_bit_k 1)` event, not one per table state): factored
models have *fewer* events, not more. Counts are preserved: the
factored reachable space is isomorphic to the original per process
(the canonical-0 convention keeps dead registers from multiplying
states), which the triangle checks per subject as always.

Flattened processes (no parameters — chiron single, peterson, relay)
get a one-coordinate candidate and pass through unfactored; recovering
their structure needs SP-partition discovery (Hartmanis–Stearns) on
the ground LTS, which is specified separately when the measurements
here justify it.

## 6. Oracles

* The composed `.hsc` runs the cegar/symbolic/explicit triangle like any
  corpus model (spec §5.7, §7): `xreach` and `cegar::mono` pin the
  induced semantics; a disagreement is a front-end bug.
* Per-process state counts are checked against LTSA's where the paper
  or the artifact states them (the Chiron dispatcher table,
  `examples/cac08/README.md`).
* The property verdict per subject is compared against CAC08's; a
  mismatch is a finding to report, never to paper over.
