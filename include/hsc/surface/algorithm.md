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
(leaf  NAME LO HI)                 ; a leaf ⟨A⟩ over int, domain [LO, HI)
(shape SORT)                       ; the top sort; every leaf used exactly once
(init  (NAME VAL)*)                ; initial word; unlisted leaves take LO
(event NAME (when ATOM*) (do ACT*))
(reach NAME [saturate|naive])      ; least fixpoint of (Σ events) from init
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
ACT  ::= (:= NAME K) | (+= NAME K) | (-= NAME K)
```

### Leaves and the shape

A `leaf` imports the `int_set` theory (the only one wired) and records the
bound `[LO,HI)`. `shape` interns the sort tree via `shape_table::pair`; `spine`
and `balanced` are desugared here, not in the parser. The left-to-right leaf
order of the shape is the frontier; each `NAME` resolves to its frontier index.
A bound is mandatory: an unguarded shift under a closure has no finite orbit
without one (Obligation 2.7 / hazard H2). Input is trusted to guard its shifts.

### An event is a product term

Per event, for each leaf the translator builds one `int_set` local term from
that leaf's atoms and its (at most one) action, then assembles the event with
`core::product`. Untouched leaves get `id`, so an event touching one leaf is a
path of the shape's depth (Thm 4.3 — skip is free, visible in the compiled
term).

Compiling a leaf's contribution, `D = interval(LO,HI)` its domain:

* **guard** = meet over the leaf's atoms, each atom filtered from `D` by
  enumeration (`==`,`in` pick values; `<`,`<=`,`>`,`>=`,`!=` filter `D`). No
  atoms ⇒ guard is *all* (the sentinel `none`, read by the theory as "no
  guard").
* if the guard is **empty**, the event can never fire ⇒ the whole event is
  dropped (never built). This sidesteps `keep(∅)`, which the theory folds to
  `id`.
* **action** (≤ 1 per leaf): `:=K` ⇒ `assign(guard,K)`; `+=K` ⇒ `shift(guard,K)`;
  `-=K` ⇒ `shift(guard,-K)`. No action but a guard ⇒ `keep(guard)`. No action
  and no guard ⇒ `id`.

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

### Queries

`(states)`, `(one-safe)` and `(deadlock)` each run their own reach and print one
MCC-format answer line. `states` prints the cardinal; on `overflow_error` (a leaf
value left its representable range) it prints `CANNOT_COMPUTE` loudly rather than
a wrong count. `one-safe` is `max_leaf_value(R) ≤ 1` — a general per-leaf
statistic of the reachable diagram, not a bound trick — and an overflow means a
place grew past 1, i.e. FALSE. `deadlock` is `R ∖ ⋃ enabled_t` nonempty (§4.7):
each event's enabling set is the cylinder of its guard sets (full declared domain
elsewhere), unioned, subtracted from `R`.
