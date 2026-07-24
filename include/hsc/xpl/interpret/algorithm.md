# `interpret/` — the transition relation

The concrete semantics of surface events: what firing one event does to
one state. States are words (`int32` per frontier position); expressions
are `lia` codes read through the factory's const API — evaluation is pure
reading, no interning traffic.

## 1. The model

    model  = { arity, events }
    event  = { name, line, guard : bexpr, clauses : clause+ }
    clause = action+
    action = assign(target, rhs : iexpr) | havoc(target, lo, hi)
    target = a frontier position
           | an array access (cell positions, index : iexpr)

The guard is the conjunction of every `when` form of the event, wherever
it stands in the body — all guards read the pre-state, as in the symbolic
compile. Clauses apply in order, sequential steps; within one clause every
read (right-hand sides *and* target indexes) sees the clause's pre-state
and every write lands simultaneously — a clause is a true multi-assign, a
swap is one clause. `+=`/`-=` are read-modify-write sugar resolved at
model build.

## 2. Firing

    fire(e, s):
      guard(e) held at s (the engine checked)
      branches = { s }
      for clause in clauses, for each branch:
        evaluate every target index and every rhs against the branch
        havoc(t, lo, hi) forks one branch per value of [lo, hi)
        two writes to one position: error (§4)
        apply the writes
      emit each branch with its changed set: the positions whose final
      value differs from s

Nondeterminism (havoc) multiplies branches; a deterministic event emits
one successor. `changed` is by value comparison — a write that restored
the old value is not a change.

## 3. Evaluation

Arithmetic runs in int64 inside an expression and is checked back into
int32 at every fold step and at every write — leaving int32 is an error,
as everywhere in libHSC (`lia`'s folds are overflow-checked and loud;
GAL's silent wrap is not kept).

⊥ (division/modulo by zero, an out-of-range shift, a negative exponent, an
out-of-bounds array read) is Kleene inside a guard: `false ∧ ⊥ = false`,
`true ∨ ⊥ = true`. This matches `lia`'s three-valued `eval_bool` — and it
must, because `lia` sorts conjuncts canonically: source order is gone, so
left-to-right short-circuit would be arbitrary. The guard idiom
`(and (> i 0) (== (at tab i) …))` works under Kleene whatever the operand
order.

A ⊥ that survives to a decision is an error: a guard whose overall value
is ⊥, an action rhs or target index that is ⊥. The evaluator records the
innermost cause ("division by zero", "index 7 out of bounds, 4 cells") so
the raised error names the culprit, not just the event.

Declared leaf bounds (`(leaf x LO HI)`) are model information, not a
runtime check — matching the symbolic engine, which never materializes a
domain. A bounds lint is a later, separate pass.

## 4. Errors

`interp_error` carries the event, the clause, the cause, and the state it
happened in (the surface prints states as runnable word literals). Raised
by: ⊥ at a decision (§3), overflow, conflicting writes in one clause. The
engine turns it into the TOP result (`../algorithm.md` §4).

## 5. Supports

`finalize()` computes per event, as sorted sparse sets over positions:

* `ctrl` — guard reads: scalar support plus every cell of every accessed
  array; what enabledness depends on. "Control" positions in the
  read-only sense: a position in `ctrl` minus `writes` is never moved by
  the event, only consulted.
* `reads` — every read anywhere in the event (guard, rhs, indexes), same
  cell convention.
* `writes` — targets, conservatively: a dynamic array target contributes
  every cell. The *actual* changed set is computed at fire time by value
  comparison — precision where it is cheap.

And the model-level inverse `readers(p)`: the events whose `ctrl`
contains `p` — the var→event relation the engine's maintenance walks
(`../algorithm.md` §3).
