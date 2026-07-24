# The `.hsc` language — syntax and semantics

A `.hsc` file describes a state machine — leaves holding integers, events
that guard and update them — plus the questions to ask of its reachable
state space. The `hsc` CLI runs one file end to end:

```
./build/tools/hsc model.hsc              # run: declarations, events, queries
./build/tools/hsc --expand model.hsc     # print the flat form of a parametric model
./build/tools/hsc -DN=20 model.hsc       # override a (param N …) from the command line
```

Exit code 0 means every `(expect …)` in the file held; nonzero means an
error or a failed expectation — **a model file is a self-checking test**.

This manual is the user view. The implementer view (how each form compiles
to the calculus) is `include/hsc/surface/algorithm.md`; the theory is the
paper draft `research_notes/hsc_core.md`.

## 1. A first model

Four places in a ring, three tokens circulating (from
`examples/models/ring4.hsc`, abbreviated):

```lisp
; comments run to end of line
(leaf a 0 4)                 ; a variable over [0,4)
(leaf b 0 4)
(leaf c 0 4)
(leaf d 0 4)
(shape (spine a b c d))      ; the tree layout of the state vector
(init (a 3))                 ; start: a=3, everything else at its LO
(event ab (when (> a 0)) (do (-= a 1) (+= b 1)))
(event bc (when (> b 0)) (do (-= b 1) (+= c 1)))
(event cd (when (> c 0)) (do (-= c 1) (+= d 1)))
(event da (when (> d 0)) (do (-= d 1) (+= a 1)))
(reach R)                    ; R := all states reachable from init
(expect R 20)                ; C(6,3) = 20 distributions — checked on exit
(select Slt R (< a c))       ; the subset where a < c …
(expect Slt 7)               ; … 7 of them
```

Forms execute in reading order. Declarations must precede use; the
translator reports errors with the source line.

## 2. Syntax

Everything is an s-expression: an atom (symbol or integer) or a
parenthesised list. `;` starts a line comment. There are no strings, no
floats, no quoting — the reader is deliberately dumb and total:

```
datum ::= ATOM | '(' datum* ')'
ATOM  ::= SYMBOL | INT
```

## 3. Declarations

```lisp
(leaf NAME [LO HI])          ; an integer variable; bounds opt-in
(array NAME CELL+)           ; group existing leaves: NAME[i] addresses them
(array NAME COUNT [LO HI])   ; parametric: declares NAME_0 … NAME_{COUNT-1}
(param NAME EXPR)            ; a compile-time integer (see §7)
(shape SORT)                 ; the tree layout; every leaf used exactly once
```

A `leaf` is a variable over **Int**. Without bounds no domain is
materialized: what keeps the state space finite is the model's own guards.
`(leaf NAME LO HI)` records a finite domain `[LO,HI)` — the initial default
becomes LO instead of 0 — but it is *not* a clamp and never silently enters
a guard. A value leaving 32-bit range is a loud overflow error, never a
silent wrap.

### The shape

```
SORT ::= unit | NAME
       | (pair SORT SORT)              ; the primitive: a binary split
       | (spine SORT+)                 ; sugar: right comb — (pair a (pair b …))
       | (balanced SORT+)              ; sugar: split in half, left-biased
       | (blocked [spine|rec] G BINDER SORT+)
                                       ; grain sugar: blocks of G components.
                                       ; default/spine: one level of blocks;
                                       ; rec: G-ary branching per level
```

The shape is the **tree layout of the state vector** — it decides sharing,
and therefore size and speed, but never the answer: any shape over the same
leaves denotes the same set of states. The left-to-right leaf order of the
shape is the *frontier*; array cells may sit anywhere in it (interleaved,
spread, permuted — access still works). Choosing a good shape is the
modeller's job: libHSC consumes a declared shape and refuses to guess.
Rules of thumb: group components that interact; `balanced` buys sharing
between isomorphic subtrees (a ring of N identical philosophers costs
O(log N) nodes); a `spine` is the classical flat layout.

## 4. States

```lisp
(init (NAME VAL)*)           ; edit the base word (every leaf at its LO)
(init EVTERM)                ; seed = the event applied ONCE to the base word
(word NAME (LEAF VAL)*)      ; bind one literal state as a named result
```

The **base word** is every leaf at its declared LO (0 if unbounded). The
pair form edits it. The event form is the general one: `alt` gives several
initial states, `havoc` gives ranges, havoc-then-`when` declares an initial
region by predicate. An init event whose image is empty is a malformed
model, refused loudly.

## 5. Events

```lisp
(event NAME (when BEXP*) (do ACT*)+)
(alt NAME EVTERM+)           ; nondeterministic choice, as a named term
(seq NAME EVTERM+)           ; sequential composition, as a named term
```

An `event` is a guarded command: the `when` forms conjoin (all read the
**pre-state**), then the `do` clauses apply **in order** — each `do` clause
is one atomic step whose assignments are simultaneous (so
`(do (:= x y) (:= y x))` is a swap). Use several `do` clauses for dependent
updates.

```
ACT ::= (:= LHS EXPR) | (+= LHS EXPR) | (-= LHS EXPR)
      | (havoc LHS LO HI)    ; any value of [LO,HI)
LHS ::= NAME | (at NAME EXPR)
```

Every `event`, `alt` and `seq` declares a *named term* usable wherever an
event term stands:

```
EVTERM ::= NAME                          ; a declared event / alt / seq
         | (alt EVTERM+) | (seq EVTERM+) ; inline composition
         | (when BEXP+) | (do ACT+)      ; anonymous filter / one clause
         | (abort)                       ; the zero term: no successors
```

`(abort)` never fires: it contributes nothing to an `alt` and kills a
`seq`. A guard that folds to false, or a statically out-of-bounds array
access, makes its event the zero term — an honest dead event, not an error.

There is no locality restriction: guards and updates may relate variables
anywhere in the shape (`(when (< a c))`, `(:= x (+ y z))`, `tab[tab[x]]`).
Local pieces compile to cheap per-leaf terms; the rest goes through the
crossing case engine. You pay for what crosses, and only where it crosses.

## 6. Expressions

```
EXPR ::= INT | NAME | (at NAME EXPR)     ; array access, any index expression
       | (OP EXPR EXPR) | (~ EXPR)       ; OP ∈ + - * / % << >> & | ^
BEXP ::= (CMP EXPR EXPR)                 ; CMP ∈ == != < <= > >=
       | (in EXPR INT+)
       | (and BEXP*) | (or BEXP*) | (not BEXP) | (imply BEXP BEXP)
```

The two kinds coerce both ways: a boolean in integer position reads 0/1, an
integer in boolean position reads `!= 0`. An out-of-bounds array access is
⊥, and an event touching ⊥ aborts (contributes no successor). Connectives
are strong-Kleene: `(and false ⊥)` is false, `(or true ⊥)` is true — the
lazy `&&`/`||` a C modeller expects.

## 7. The parametric layer

Before translation, a rewrite pass eliminates the parametric forms — the
translator never sees them. `--expand` prints its output: flat, diffable,
runnable `.hsc`.

```lisp
(param N 5)                  ; a named compile-time integer; -DN=… overrides
(array st N 0 3)             ; N cells st_0 … st_{N-1} over [0,3)
(forall (i N) DATUM+)        ; generate for i = 0 … N-1  (conjunctive)
(forall (i LO HI) DATUM+)    ; i over [LO,HI); bounds may use outer binders
(exists (i N) DATUM+)        ; the disjunctive twin
```

A binder generates its body once per index value; **the position decides
how the copies combine**. `forall` uses the context's conjunctive
combinator, `exists` the disjunctive one:

| context | `forall` | `exists` |
|---|---|---|
| event body | clauses conjoin (one event) | one event **per index** (`NAME_i`, a family) |
| inside `when` | `and` | `or` |
| EVTERM position | `seq` | `alt` |
| `shape` / `init` / top level | splice in place | — |

Index arithmetic is grounded and folded at expansion: `(at f (% (+ i 1) N))`
becomes the literal cell. Dining philosophers, complete
(`examples/param/philo.hsc`):

```lisp
(param N 5)
(array st N 0 3)
(array f N 0 2)
(shape (spine (forall (i N) (at st i) (at f i))))
(init)
(event take1
  (exists (i N)
    (when (== (at st i) 0) (== (at f i) 0))
    (do (:= (at f i) 1) (:= (at st i) 1))))
...
(reach R saturate)
(expect R 82)
```

**Certified uniform families.** An event whose whole body is one `exists`
binder used uniformly (index only as an array access at a constant offset
mod N, full range) is recognized and built by recursion over the shape
instead of being enumerated — O(log N) instead of O(N) term codes. The
environment variable `HSC_FAMILY` controls it: `check` (default) builds
both routes and requires the identical code; `declared` trusts the
certificate; `unfold` always enumerates.

## 8. Computing and querying

```lisp
(reach NAME [saturate|naive] [EVTERM] [from RESULT])
(apply NAME EVTERM SOURCE)   ; one-step image, no closure
(select NAME SOURCE QATOM+)  ; the subset satisfying every atom
(count NAME)                 ; state count (a double — it is an order of magnitude)
(nodes NAME)                 ; diagram nodes — the representation size
(max-value NAME)             ; largest value any leaf holds in NAME
(print NAME)
(get-witness NAME)           ; one state, printed as a re-runnable (word …)
(get-states NAME [K])        ; up to K states (default 10), one per line
(expect NAME N)              ; assert count == N; nonzero exit on miss
(states [NAME])              ; the count in MCC output format
(bill)                       ; meters: nodes, terms, caches, time
```

`reach` computes the least fixpoint of the event term from a bound result
(default: the seed). Without an explicit EVTERM it uses the `alt` of every
declared `(event …)`. `saturate` (the default) is the fast schedule;
`naive` iterates one step at a time — same answer, so `naive` doubles as a
differential oracle in tests.

`select` atoms are boolean forms over leaves — separable atoms
(`(== a 0)`, `(in x 1 3)`) descend directly; comparisons between two
leaves (`(< a c)`) and quantified crossers resolve through the case
engine, whatever the shape. Results bind to names and compose: `reach`
from a `select`, `apply` to a `word`, `select` of a `select`.

## 9. Errors, honestly

* Parse, expansion and translation errors carry the source line.
* An **unbounded orbit diverges honestly**: `(event up (when) (do (+= x 1)))`
  under `reach` never terminates — guards are what bound orbits (see
  `HAZARDS.md` H2). A value leaving int32 raises `overflow_error`;
  `(states)` then prints `CANNOT_COMPUTE` rather than a wrong count.
* `count` is a `double`: exact up to ~15 significant digits, an order of
  magnitude beyond.

## 10. Where models come from

Besides writing them by hand:

* `dve2hsc` — imports DVE (BEEM) models; the 276-model corpus under
  `examples/divine/` is its output.
* `nupn2hsc` / `hsc-mcc` — import PNML/NUPN Petri nets (the NUPN unit tree
  becomes the shape; a Louvain decomposition can propose one); `hsc-mcc`
  answers MCC examinations directly. Set: `examples/mcc/`.
* `tools/hanoi.py` — a tiny generator emitting `.hsc` text; loops belong
  in generators, the reader stays dumb.
