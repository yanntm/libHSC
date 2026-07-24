# `interpret/` — the transition relation

The concrete semantics of events: one event at one state yields **zero or
more successors** — zero meaning not actually enabled. States are words
(`int32` per frontier position); expressions are `lia` codes read through
the factory's const API — evaluation is pure reading, no interning
traffic.

## 1. The model

    model  = { arity, terms, events }

    term  ::= filter(bexpr)      # test: keeps the branches where it holds
            | update(action+)    # one simultaneous multi-assign
            | seq(term+)         # sequential composition
            | alt(term+)         # nondeterministic choice
            | abort              # the zero term: kills every branch

    action = assign(target, rhs : iexpr) | havoc(target, lo, hi)
    target = a frontier position
           | an array access (cell positions, index : iexpr)

    event  = { name, line, root : term, quick : bexpr }

The general form is kept: tests may sit mid-sequence and read the state
*there*, `alt` forks, `abort` kills. No determinism, no guard/action shape
is assumed. `lfp` (a closure inside an event) is deliberately absent from
the grammar for now.

The comfortable special case is *looked for, not required*: the surface's
plain `(event NAME (when …) (do …)+)` builds `seq(filter, update+)` with
every `when` hoisted into one pre-state filter — matching the symbolic
compile, where all guards read the pre-state — and `quick` is then that
guard, exact. For a general term, `quick` is any *necessary* enabling
condition (a filter prefix that hoists; `btrue` when nothing is known).
The engine's enabled sets are **may-fire** sets over `quick`; the
interpreter is authoritative — a fired event may produce nothing.

Within one update every read (right-hand sides *and* target indexes) sees
the update's pre-state and every write lands simultaneously — an update is
a true multi-assign, a swap is one update. `+=`/`-=` are read-modify-write
sugar resolved at model build.

## 2. Firing

    fire(e, s): interpret root over branches = { s }
      filter g : keep the branches where g holds     # on the branch state
      update   : per branch — evaluate every target index and every rhs
                 against the branch; havoc(t, lo, hi) forks one branch per
                 value of [lo, hi); two writes to one position: error (§4);
                 apply the writes

Havoc is not a primitive of the algebra: it *is* the `alt` of the range's
assignments, kept as one action so the range is never materialized in the
term — the fire-time fork is that alt taken lazily, and inside a
simultaneous clause the alt distributes over the other assigns (which is
what forking a branch copy does).
      seq      : the kids in order (no branches left → done)
      alt      : each kid interprets a copy of the branches; the results
                 concatenate
      abort    : no branches
      emit each surviving branch with its changed set: the positions
      whose final value differs from s

`changed` is by value comparison — a write that restored the old value is
not a change. Duplicate successors (two `alt` kids with one effect) are
harmless: the store deduplicates.

## 3. Evaluation

Arithmetic runs in int64 inside an expression and is checked back into
int32 at every fold step and at every write — leaving int32 is an error,
as everywhere in libHSC (`lia`'s folds are overflow-checked and loud;
GAL's silent wrap is not kept).

⊥ (division/modulo by zero, an out-of-range shift, a negative exponent, an
out-of-bounds array read) is Kleene inside a test: `false ∧ ⊥ = false`,
`true ∨ ⊥ = true`. This matches `lia`'s three-valued `eval_bool` — and it
must, because `lia` sorts conjuncts canonically: source order is gone, so
left-to-right short-circuit would be arbitrary. The guard idiom
`(and (> i 0) (== (at tab i) …))` works under Kleene whatever the operand
order.

A ⊥ that survives to a decision is an error: a filter whose overall value
is ⊥, an action rhs or target index that is ⊥. The evaluator records the
innermost cause ("division by zero", "index 7 out of bounds, 4 cells") so
the raised error names the culprit, not just the event.

Declared leaf bounds (`(leaf x LO HI)`) are model information, not a
runtime check — matching the symbolic engine, which never materializes a
domain. A bounds lint is a later, separate pass.

## 4. Errors

`interp_error` carries the event, the cause, and the state it happened in
(the surface prints states as runnable word literals). Raised by: ⊥ at a
decision (§3), overflow, conflicting writes in one update. The engine
turns it into the TOP result (`../algorithm.md` §4).

## 5. Supports

`finalize()` computes per event, as sorted sparse sets over positions:

* `ctrl` — the support of `quick` (scalars plus every cell of accessed
  arrays): what the may-fire status depends on, hence what the engine's
  maintenance watches. Constant `quick` has empty `ctrl`: such an event is
  resident in every may-fire set and the interpreter alone decides.
* `reads` — every read anywhere in the term (filters, rhs, indexes), same
  cell convention. Read-only positions — `reads` minus `writes` — are the
  "control" refinement available to later strategies.
* `writes` — targets, conservatively: a dynamic array target contributes
  every cell. The *actual* changed set is computed at fire time by value
  comparison — precision where it is cheap.

And the model-level inverse `readers(p)`: the events whose `ctrl`
contains `p` — the var→event relation the engine's maintenance walks
(`../algorithm.md` §3).
