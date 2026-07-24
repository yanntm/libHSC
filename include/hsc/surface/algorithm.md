# surface — grammar and compile map

Three passes. §1 is the parser's whole world (syntax); §1b the parametric
expander's (a `datum → datum` rewrite); §2 the translator's (meaning). They
share only the `datum` tree.

## 1. Syntax (T2M): text → `datum`

An s-expression is an atom or a parenthesised list. Atoms are symbols or
integers; `;` starts a line comment. The parser emits a flat sequence of
top-level **forms** (each a `datum`) and nothing else — it does not know which
symbols are keywords.

```
datum ::= ATOM | '(' datum* ')'
ATOM  ::= SYMBOL | INT
```

That is the entire parser contract. Everything below is the translator reading
structure into these trees.

## 1b. The parametric pass (`expand.hh`)

Between parser and translator, `expand` rewrites parametric forms away; the
translator never sees them. It is the identity on binder-free input.

```
(param NAME EXPR)            ; compile-time integer, usable where INT stands;
                             ; consumed by the pass
(array NAME COUNT [LO HI])   ; COUNT folds to an INT: lowers to the leaves
                             ; NAME_0 … NAME_{COUNT-1} plus the explicit-cell
                             ; (array …) grouping them
(forall (I HI) DATUM+)       ; a binder generates a list in place, I over
(forall (I LO HI) DATUM+)    ; [LO,HI); bounds fold to INTs and may use
(exists (I …) DATUM+)        ; enclosing binders (dependent ranges)
```

A binder carries no semantics: the position of the generated list decides
its meaning, as for explicit lists. Each context has a conjunctive and a
disjunctive combinator; `forall` takes the first, `exists` the second — a
splice where the context's combinator matches, a wrapped node where not:
seq/alt in EVTERM positions, and/or in BEXP positions, clause splice in an
event body (`forall`) versus an event *family* (`exists` lifts: one plain
event per index value, named `NAME_v`, each entering the default system
sum), action splice inside one `do` clause (`forall` only — the clause
stays one simultaneous multi-assign), sort splice under `spine`/`balanced`,
pair splice in `init`/`word`, form splice at top level (`forall` only).
Empty ranges: identity `(when)` for `forall`, `(abort)` for `exists`, in
wrap positions; nothing under a splice.

Substitution grounds in-scope names to integer atoms (name capture is
refused: params and indexes may not collide with any declared name);
constant arithmetic folds bottom-up, so `(% (+ i 1) N)` becomes a literal;
a grounded index into a *generated* array becomes its cell atom `NAME_K`
(out of range is refused — the bound is known), which is what lets
generated cells stand in `shape` and pair positions. Everything else —
degenerate guards folding to false, dynamic indices, ⊥ — keeps its
existing translator semantics.

## 2. Meaning (M2M): forms → operations

The translator walks the forms in order, maintaining: the leaf declarations, the
top shape and a name→frontier-index map, the events, and a table of named result
diagrams. Declarations must precede the events that use them; generated input is
well-ordered, and the translator errors (with a line) if it is not.

### Forms

```
(leaf  NAME [LO HI])               ; a leaf ⟨A⟩ over Int; bound opt-in
(array NAME CELL+)                 ; the named leaves, in index order, are
                                   ; addressable as NAME[i] — placed anywhere
(shape SORT)                       ; the top sort; every leaf used exactly once
(init  (NAME VAL)*)                ; edit the base word (all leaves at LO)
(init  EVTERM)                     ; seed = EVTERM applied ONCE to the base;
                                   ; an empty image is a malformed model
(event NAME (when BEXP*) (do ACT*)+)   ; several (do …): sequential steps
(alt NAME EVTERM+)                 ; nondeterministic choice, a named term
(seq NAME EVTERM+)                 ; sequential composition, a named term
(word NAME (LEAF VAL)*)            ; bind one state; unlisted leaves at LO
(reach NAME [saturate|naive] [EVTERM] [from RESULT])
                                   ; least fixpoint of EVTERM from a bound
                                   ; result (default: the seed); default
                                   ; EVTERM: the ALT of every (event …)
(apply NAME EVTERM SOURCE)         ; one-step image, no closure
(get-witness NAME)                 ; one state of NAME, as a word literal
(get-states NAME [K])              ; up to K states (10), one per line
(select NAME SOURCE QATOM+)        ; subset of SOURCE satisfying every QATOM
(count NAME) (nodes NAME) (print NAME)
(max-value NAME)                   ; largest value any leaf holds in NAME
(expect NAME N)                    ; assert cardinal == N; nonzero exit on miss
(bill)                             ; meters: nodes, terms, caches, time
(states [NAME])                    ; cardinal, MCC format; no arg: default reach

SORT ::= unit | NAME
       | (pair SORT SORT)
       | (spine SORT+)             ; sugar: (pair a (pair b (… unit)))
       | (balanced SORT+)          ; sugar: split in half, left-biased
       | (blocked [spine|rec] G BINDER SORT+)  ; grain sugar (parametric
                                   ; pass): libITS scalar-set grouping as
                                   ; shape constructors. Default/spine
                                   ; (DEPTH1): one level of consecutive
                                   ; blocks of G components, balanced of
                                   ; balanced (resp. spine of spine), last
                                   ; block short when G ∤ range. rec
                                   ; (DEPTHREC): G-ary branching per level,
                                   ; recursively, flat below G instances;
                                   ; rec 2 = component-grouped balanced.
EVTERM ::= NAME                          ; a declared event / alt / seq
       | (alt EVTERM+) | (seq EVTERM+)   ; inline composition
       | (when BEXP+) | (do ACT+)        ; anonymous filter / one clause —
                                         ; a guarded command is their seq
       | (abort)                         ; the zero term: no successors —
                                         ; nothing in an alt, death in a seq
EXPR ::= INT | NAME | (at NAME EXPR)     ; an array access, any index
       | (OP EXPR EXPR) | (~ EXPR)       ; OP ∈ + - * / % << >> & | ^
BEXP ::= (CMP EXPR EXPR) | (in EXPR INT+)      ; CMP ∈ == != < <= > >=
       | (and BEXP*) | (or BEXP*) | (not BEXP) | (imply BEXP BEXP)
QATOM ::= BEXP | (CMP NAME NAME)
ACT  ::= (:= LHS EXPR) | (+= LHS EXPR) | (-= LHS EXPR)
       | (havoc LHS K K)                 ; any value of [lo, hi): the ALT
                                         ; of the range's assignments, one
                                         ; theory term
LHS  ::= NAME | (at NAME EXPR)
```

The two expression kinds coerce both ways, as in GAL: a boolean form in
integer position reads 0/1, an integer form in boolean position reads
`!= 0`. Expressions land in the `lia` interchange theory over frontier
positions; an array access carries the placement of its cells in the node
(`lia/algorithm.md`), so cells sit anywhere the shape puts them.

### Leaves and the shape

A `leaf` imports the `int_set` theory (the only one wired). The type is
**Int**: no domain is required or materialized. `(leaf NAME LO HI)` opts into
a finite domain — recorded as model information (the init default becomes LO
instead of 0), available to a compiler that wants an enumeration; it is not a
clamp and never silently enters a guard. `shape` interns the sort tree via
`shape_table::pair`; `spine` and `balanced` are desugared here, not in the
parser. The left-to-right leaf order of the shape is the frontier; each
`NAME` resolves to its frontier index. What keeps an orbit finite is the
model's own guards (hazard H2): an unguarded shift under a
closure diverges honestly — a value leaving int32 is a loud
`overflow_error`, never a silent wrap.

### An event is a composition; its separable pieces are products

Guards and assignments are arbitrary expressions. The compiler splits each
event along the separable/crossing line and composes the pieces in application order:

1. **Crossing filters first.** Every `when` form whose support is more than
   one position (or touches an array) becomes a case bracket
   (`case_engine::make_event`, guard only) — applied before any action, so
   every guard reads the pre-state.
2. **One fused product.** Single-position `when` forms conjoin per leaf;
   the first `do` clause, when separable, fuses its assignments in:
   `apply_if(guard, rhs)` at each touched leaf (which folds to
   assign/shift/keep where the expression is one), `keep_if(guard)` where
   only a guard stands, `id` elsewhere. Untouched leaves cost nothing
   (skip is free, visible in the compiled term).
3. **Each further `do` clause**, an atomic sequential step: a product of
   per-leaf `apply_if` when every assignment writes one position that at
   most itself is read; otherwise the *whole clause* becomes one case
   bracket — its assignments stay simultaneous and its reads pre-clause.
   Sequential composition remains the preferred encoding of dependent
   updates; a single clause is a true synchronous multi-assign (a swap).

A guard that **folds to `false`** (or ⊥), or a statically out-of-bounds
access, makes the event the *zero term* (never fires: nothing in an `alt`,
death in a `seq`); a guard that merely never holds in practice is an
honest never-firing term. A separable event compiles exactly as it always
did — products decompose into the F/L saturation schedule — and a
crossing piece is an ordinary `G` event: `split_equiv` at the cut, curry
the residual, cached by its interned term (`hsc/event.hh`).

### The event algebra

Every `event`, `alt` and `seq` declares a *named term*; `alt` is
`op_table::sum` (nondeterministic choice — commutative, idempotent), `seq`
is `compose` in reading order. The whole system is itself just a term:
`reach` takes one, and without an argument uses the ALT of every declared
`event` — the historical behavior as sugar. Saturation flattens nested
`alt`s into its summand list, so the schedule sees through the
composition; queries (`states`, `deadlock`, …) run the default system.

### reach

`(reach R)` sums the events (`op_table::sum`) and computes the least fixpoint of
`X ↦ X ∪ H·X` from `init`. `saturate` (default) uses `core::saturate`; `naive`
iterates `apply_local` to a fixed point. Both denote the same diagram, so `naive` is the differential oracle for `saturate`. `expect`
turns a file into a self-checking oracle.

### select

`(select NAME SOURCE QATOM+)` filters the stored result `SOURCE` by a
conjunction of query atoms, applied left to right, and stores the subset under
`NAME`. A `QATOM` with a constant right-hand side (or `in`) is separable: a
descent to that frontier position and a meet with the atom's set
(`select_in`). A right-hand side naming a second leaf is the crossing
comparison `x ⋈ y` (⋈ any of the six comparators): resolved by the crossing case
(`select_compare`, `hsc/query.hh`) — at the cut separating the two positions,
`split_equiv` the head coordinate and curry the residual per class onto the
tail. The casing is honest: no rewriting by domain knowledge (a one-safe
`a <= b` is *not* turned into `a==0 ∨ b==1`); each head value gets its class,
each class its restricted tail. The coordinates may sit at any depth on
either side of the cut — a coordinate below the head splits the head
*subdiagram* (`split_equiv` on diagrams, the same act one level in), so the
same flat-name query resolves under any shape: spine, balanced, or a
decomposed unit tree. Events take the same crossing forms through the case
engine (`hsc/event.hh`); `select` keeps its own two-leaf path for queries.

### The state layer

A state is a bound result like any other. The **base word** is every leaf
at its declared LO (0 unbounded), edited by pair `init`s. An **init
event** seeds by applying an event term once to the base — specification
level, not a step of the transition relation: `alt` gives several initial
states, `havoc` ranges, and havoc-then-`when` declares an initial region
by predicate. An init event whose image is empty is a malformed model,
refused loudly. `word` binds a literal state; `reach … from R` closes
from any bound result; `apply` is the one-step image. `get-witness`
exhibits one state — the SMT `get-model` analogue, a nonempty result
being sat — and `get-states` enumerates boundedly; both print **word
literals**, so every exhibited state is re-runnable text.

### Queries

Queries compose over bound results: `(reach x SYSTEM)` binds, `count` /
`nodes` / `max-value` / `select` / `expect` consume. `max-value` is the
per-leaf maximum of a result (MAX_TOKEN_IN_PLACE; 1-safety is
`max-value ≤ 1`, judged by whoever asked — the MCC protocol lives in the
runners, `hsc-mcc`, not here). `(states [NAME])` prints an MCC-format
cardinal, running the default reach when no result is named; on
`overflow_error` (a leaf value left its representable range) it prints
`CANNOT_COMPUTE` loudly rather than a wrong count. The former `one-safe`
and `deadlock` forms are gone: the first was the Petri front end's
question in surface clothing, the second assumed every refusal sits in
`when` — untrue since abort-bearing events (⊥) arrived.
