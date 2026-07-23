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
(shape SORT)                       ; the top sort; every leaf used exactly once
(init  (NAME VAL)*)                ; initial word; unlisted leaves take LO
(event NAME (when ATOM*) (do ACT*))
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
ATOM ::= (CMP NAME K) | (in NAME K+)     ; CMP ∈ == != < <= > >=
QATOM ::= ATOM | (CMP NAME NAME)         ; two leaves: the §7 crossing case
ACT  ::= (:= NAME K) | (+= NAME K) | (-= NAME K)
```

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

### An event is a product term

Per event, for each leaf the translator builds one `int_set` local term from
that leaf's atoms and its (at most one) action, then assembles the event with
`core::product`. Untouched leaves get `id`, so an event touching one leaf is a
path of the shape's depth (Thm 4.3 — skip is free, visible in the compiled
term).

Compiling a leaf's contribution — guards are **symbolic** (`lia` expressions
over the coordinate, evaluated on the values present; nothing precomputed):

* **guard** = conjunction over the leaf's atoms, each a `bexpr` on the
  coordinate (`(< x K)` ⇒ `v0 < K`; `(in x K…)` ⇒ a disjunction of `==` —
  an enumeration the model itself wrote). No atoms ⇒ `true`.
* a guard that **folds to `false`** (contradictory constants) drops the whole
  event; a guard that merely never holds in practice is an honest
  never-firing term.
* **action** (≤ 1 per leaf): `:=K` ⇒ `assign_if(guard,K)`; `+=K` ⇒
  `shift_if(guard,K)`; `-=K` ⇒ `shift_if(guard,-K)`. No action but a guard ⇒
  `keep_if(guard)`. No action and no guard ⇒ `id`.

### Separability check (the §6/§7 line)

Each atom names exactly one leaf; each action's value is a constant. An atom or
action reaching a second leaf, or an `a[i]` form, is **crossing**: the
translator refuses it by name — "needs `split_equiv` (§7), not implemented" —
having parsed and understood it. Disjunction across leaves is not a product
term either; the generator expands it into several `event`s.

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
decomposed unit tree. One limit remains, refused with a message: events
still take plain `ATOM`s only — crossing *updates* (`x := y + z`) are the
next §7 step.

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
