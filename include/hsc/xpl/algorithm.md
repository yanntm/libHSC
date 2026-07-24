# `xpl/` — algorithms

The engine: enumerative reachability over the interpreted transition
relation (`interpret/algorithm.md`). Self-contained: every definition the
code relies on is stated here.

## 1. States and the store

A state is a word: one `int32` per frontier position, `arity` of them, in
shape order — exactly the `env` that `lia` evaluation reads. The store
keeps every distinct state once: rows of width `arity` in one growing
array, an open-addressed table of ids over them. Insert-if-absent returns
(id, fresh); ids are dense, in allocation order. Views into the store are
invalidated by the next insert — the engine copies a state out before
expanding it.

## 2. Reachability

Breadth-first from the seeds:

    reach(model, seeds):
      for s in seeds: intern; if fresh, queue (id, enabled(s) by full sweep)
      while queue:
        (id, E) = pop; s = copy of store[id]
        for e in E:
          for (s', changed) in fire(e, s):        # havoc branches
            (id', fresh) = intern(s'); if not fresh: continue
            stop CAPPED if the store passed the cap
            queue (id', derive(E, changed, s'))
      OK: the store is the reachable set

The result carries the store, the count, the fired-transition tally, and a
status: `ok`; `capped` — the cap bounds *stored states* (default 2²⁰):
explicit is for small instances, and the cap is the honest refusal, never a
silent truncation; or `error` (§4).

## 3. Enabled-set maintenance

The point: firing an event changes a handful of positions, so almost every
guard that held before holds after. Recomputing enabledness from scratch is
Θ(|events|) per state; maintenance is Θ(|changed| · fan-in). At 10k
philosophers — ~50k events, a fired event touching two or three positions
read by a dozen guards — a step costs dozens of guard evaluations, not
fifty thousand.

Machinery, built once in `model.finalize()`:

* `ctrl(e)` — the positions the guard of `e` reads: its scalar support
  plus every cell of every array it accesses (the index is dynamic; all
  cells are conservatively readable).
* `readers(p)` — the inverse: the events whose `ctrl` contains `p`. The
  var→event relation is stored; no event×event matrix is ever formed.

Both are sorted sparse sets (`SparseBoolArray`): unions and intersections
are linear merges, membership is binary search.

The invariant: enabledness of `e` at `s` is a function of `s` restricted
to `ctrl(e)`. Firing produced `changed` — the positions whose value
actually differs (a write that rewrote the old value is not a change). So:

    derive(E, changed, s'):
      dirty = ∪ { readers(p) | p in changed }     # sorted union
      E'    = merge over ascending event id:
                id in dirty        → guard(id, s')   # re-evaluated
                id in E \ dirty    → kept
                otherwise          → absent

Events outside `dirty` keep their status from `E` unchanged — that is the
whole invariant. Seeds pay the one full sweep. The enabled set travels with
the frontier entry and dies at expansion; nothing per-state is retained
beyond the store row.

This is the ITS-tools `WalkUtils.updateEnabled` scheme re-expressed:
`readers` plays its transposed flow matrix, with the Petri `state ≥ Pre`
test generalized to a guard evaluation — hence dirty events are re-checked
rather than all currently-enabled ones.

Refinements deliberately deferred, measured before adopted:

* **Effect classes.** Group events with identical update terms (interned
  codes make the grouping a table over code tuples): all members produce
  the same successor, so one firing serves the class, and the class with
  empty effect is stutter, skippable in reach. WalkUtils' `behaviors`.
* **Directional wake-up.** A guard atom `x ≥ k` can only *become* true
  when `x` grows: filtering `readers(p)` by the sign of the change needs
  per-atom direction data. Monotone guards are a Petri specialty; the
  general form is per-comparison, not per-event.

## 4. Errors: loud, precise, and terminal

Explicit is the debugger; nothing folds away. A division or modulo by
zero, an out-of-bounds array read or write, an overflow leaving int32, a
shift out of range, conflicting writes to one position in one clause, a
guard that decides on ⊥ — each raises immediately with the event, the
clause, the cause, and the state it happened in. The engine stops and the
result is TOP: status `error`, diagnostic and witness state attached,
usable by nothing downstream (an `expect` against it fails). The symbolic
engine may legitimately produce a count for the same model — where it
folds ⊥ into a non-firing, explicit reports the model bug instead. That
asymmetry is the feature.

## 5. Search order

FIFO expansion = breadth-first: shortest witnesses first, and the natural
seam for later strategies (guided walks, random simulation, DFS) — a
strategy owns the queue discipline, nothing else changes.
