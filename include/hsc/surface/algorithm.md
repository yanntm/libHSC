# surface — grammar and compile map

Two passes. §1 is the parser's whole world (syntax); §2 is the translator's
(meaning). They share only the `datum` tree.

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
(init  (NAME VAL)*)                ; initial word; unlisted leaves take LO
(event NAME (when BEXP*) (do ACT*)+)   ; several (do …): sequential steps
(reach NAME [saturate|naive])      ; least fixpoint of (Σ events) from init
(select NAME SOURCE QATOM+)        ; subset of SOURCE satisfying every QATOM
(count NAME) (nodes NAME) (print NAME)
(expect NAME N)                    ; assert cardinal == N; nonzero exit on miss
(bill)                             ; meters: nodes, terms, caches, time
(states)                           ; MCC StateSpace: STATE_SPACE STATES <n>
(one-safe)                         ; MCC OneSafe: FORMULA OneSafe TRUE|FALSE
(deadlock)                         ; MCC ReachabilityDeadlock: FORMULA … TRUE|FALSE

SORT ::= unit | NAME
       | (pair SORT SORT)
       | (spine SORT+)             ; sugar: (pair a (pair b (… unit)))
       | (balanced SORT+)          ; sugar: split in half, left-biased
EXPR ::= INT | NAME | (at NAME EXPR)     ; an array access, any index
       | (OP EXPR EXPR) | (~ EXPR)       ; OP ∈ + - * / % << >> & | ^
BEXP ::= (CMP EXPR EXPR) | (in EXPR INT+)      ; CMP ∈ == != < <= > >=
       | (and BEXP*) | (or BEXP*) | (not BEXP) | (imply BEXP BEXP)
QATOM ::= BEXP | (CMP NAME NAME)
ACT  ::= (:= LHS EXPR) | (+= LHS EXPR) | (-= LHS EXPR)
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
model's own guards (Obligation 2.7 / hazard H2): an unguarded shift under a
closure diverges honestly — a value leaving int32 is a loud
`overflow_error`, never a silent wrap.

### An event is a composition; its separable pieces are products

Guards and assignments are arbitrary expressions. The compiler splits each
event along the §6/§7 line and composes the pieces in application order:

1. **Crossing filters first.** Every `when` form whose support is more than
   one position (or touches an array) becomes a §7 case bracket
   (`case_engine::make_event`, guard only) — applied before any action, so
   every guard reads the pre-state.
2. **One fused product.** Single-position `when` forms conjoin per leaf;
   the first `do` clause, when separable, fuses its assignments in:
   `apply_if(guard, rhs)` at each touched leaf (which folds to
   assign/shift/keep where the expression is one), `keep_if(guard)` where
   only a guard stands, `id` elsewhere. Untouched leaves cost nothing
   (Thm 4.3 — skip is free, visible in the compiled term).
3. **Each further `do` clause**, an atomic sequential step: a product of
   per-leaf `apply_if` when every assignment writes one position that at
   most itself is read; otherwise the *whole clause* becomes one case
   bracket — its assignments stay simultaneous and its reads pre-clause.
   Sequential composition remains the preferred encoding of dependent
   updates; a single clause is a true synchronous multi-assign (a swap).

A guard that **folds to `false`** (or ⊥), or a statically out-of-bounds
access, drops the whole event; a guard that merely never holds in practice
is an honest never-firing term. A separable event compiles exactly as it
always did — products decompose into the F/L saturation schedule — and a
crossing piece is an ordinary `G` event: `split_equiv` at the cut, curry
the residual, cached by its interned term (`hsc/event.hh`). Disjunction
across leaves is still not a product term; the generator expands it into
several `event`s (surface `alt` is a planned combinator, not yet a form).

### reach

`(reach R)` sums the events (`op_table::sum`) and computes the least fixpoint of
`X ↦ X ∪ H·X` from `init`. `saturate` (default) uses `core::saturate`; `naive`
iterates `apply_local` to a fixed point. Both denote the same diagram
(Prop 5.3), so `naive` is the differential oracle for `saturate`. `expect`
turns a file into a self-checking oracle.

### select

`(select NAME SOURCE QATOM+)` filters the stored result `SOURCE` by a
conjunction of query atoms, applied left to right, and stores the subset under
`NAME`. A `QATOM` with a constant right-hand side (or `in`) is separable: a
descent to that frontier position and a meet with the atom's set
(`select_in`). A right-hand side naming a second leaf is the crossing
comparison `x ⋈ y` (⋈ any of the six comparators): resolved by the §7 case
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

### Queries

`(states)`, `(one-safe)` and `(deadlock)` each run their own reach and print one
MCC-format answer line. `states` prints the cardinal; on `overflow_error` (a leaf
value left its representable range) it prints `CANNOT_COMPUTE` loudly rather than
a wrong count. `one-safe` is `max_leaf_value(R) ≤ 1` — a general per-leaf
statistic of the reachable diagram, not a bound trick — and an overflow means a
place grew past 1, i.e. FALSE. `deadlock` is `R ∖ ⋃ enabled_t` nonempty (§4.7):
per event, `R` filtered through its guard expressions position by position
(`select_where`), unioned over events, subtracted from `R`. No domain
cylinder: only values `R` holds are consulted, so a state outside a declared
domain is judged by the guards like any other.
