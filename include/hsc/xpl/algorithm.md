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

## 2. Exploration

Breadth-first from the seeds, visitor-driven — the engine reports, the
client keeps:

    explore(model, seeds, on_state):
      admit(id) = stop CAPPED if the store passed the cap
                  stop STOPPED if on_state(id, view) says stop
                  else queue (id, its may-fire set)
      for s in seeds: intern; if fresh, admit (full sweep for the set)
      while queue:
        (id, E) = pop; s = copy of store[id]
        for e in E:                               # the may-fire set, §3
          for (s', changed) in fire(e, s):        # 0..many successors
            (id', fresh) = intern(s'); if not fresh: continue
            admit (derive(E, changed, s') for the set)
      OK: every reachable state was visited

`on_state` runs once per fresh state (seeds included), in discovery order,
with a view that dies at the next insert — the client copies what it wants
to keep, and its `stop` verdict ends the run (on-the-fly checking: found
the target, stop). The store itself exists for termination — it is the
dedup set BFS needs regardless — and is discarded unless asked for:
`reach` is the keep-everything wrapper whose result carries the store as
the reachable set.

The stats carry the count, the fired-successor tally, and a status: `ok`;
`stopped` (a visited prefix, by the visitor's verdict); `capped` — the cap
bounds *stored states* (default 2²⁰): explicit is for small instances, and
the cap is the honest refusal, never a silent truncation; or `error` (§4).

## 3. May-fire set maintenance

Each event carries a `quick` test — a necessary enabling condition, exact
for the guard/deterministic shape, `true` when nothing cheaper is known
(`interpret/algorithm.md` §1). The frontier carries per state the
**may-fire set** `E = { e : quick(e, s) }`; expansion fires exactly those,
and a fired event may still produce nothing — the interpreter is
authoritative.

The point of maintaining `E`: firing an event changes a handful of
positions, so almost every `quick` that held before holds after.
Recomputing from scratch is Θ(|events|) per state; maintenance is
Θ(|changed| · fan-in). At 10k philosophers — ~50k events, a fired event
touching two or three positions read by a dozen guards — a step costs
dozens of guard evaluations, not fifty thousand.

Machinery, built once in `model.finalize()`:

* `ctrl(e)` — the positions `quick(e)` reads: its scalar support plus
  every cell of every array it accesses (the index is dynamic; all cells
  are conservatively readable). A constant `quick` has empty `ctrl`: the
  event is resident in every may-fire set, never rechecked.
* `readers(p)` — the inverse: the events whose `ctrl` contains `p`. The
  var→event relation is stored; no event×event matrix is ever formed.

Both are sorted sparse sets (`SparseBoolArray`): unions and intersections
are linear merges, membership is binary search.

The invariant: `quick(e, s)` is a function of `s` restricted to `ctrl(e)`.
Firing produced `changed` — the positions whose value actually differs (a
write that rewrote the old value is not a change). So:

    derive(E, changed, s'):
      dirty = ∪ { readers(p) | p in changed }     # sorted union
      E'    = merge over ascending event id:
                id in dirty        → quick(id, s')   # re-evaluated
                id in E \ dirty    → kept
                otherwise          → absent

Events outside `dirty` keep their status from `E` unchanged — that is the
whole invariant. Seeds pay the one full sweep. The may-fire set travels
with the frontier entry and dies at expansion; nothing per-state is
retained beyond the store row.

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
shift out of range, conflicting writes to one position in one update, a
test that decides on ⊥ — each raises immediately with the event, the
cause, and the state it happened in. The engine stops and the
result is TOP: status `error`, diagnostic and witness state attached,
usable by nothing downstream (an `expect` against it fails). The symbolic
engine may legitimately produce a count for the same model — where it
folds ⊥ into a non-firing, explicit reports the model bug instead. That
asymmetry is the feature.

## 5. Search order

FIFO expansion = breadth-first: shortest witnesses first, and the natural
seam for later strategies (guided walks, random simulation, DFS) — a
strategy owns the queue discipline, nothing else changes.
