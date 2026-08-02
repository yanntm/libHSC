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
bounds *stored states* (default 10⁸, a memory backstop: a few million
states is normal fare, a driver's timeout is the practical limit), and is
the honest refusal, never a silent truncation; or `error` (§4).

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

## 5. Domain inference — a decoration step

A static pass over the model (`domains.hh`): which variables can be
*shown* to have a small effective domain, from the spec alone. No search
is run.

The unit of analysis is a scalar position or a whole array — declared
arrays and every array node's cell list union-find into one unit; an
array gets **one** domain for all its cells, never refined per cell.

Per unit, an abstract value in the lattice

    bot  ⊑  set(V)  (|V| ≤ cap)  ⊑  interval[lo,hi]  ⊑  top

fed by the assignments that target it, each classified **once**,
structurally — no expression evaluation, no widening. The
classification runs as a walk of every event's term tree carrying the
live **guard constraints**: filters contribute var–constant atoms
(`x ⋈ c`, either operand order, conjunctions only); an update reads
its pre-state and then kills the constraints of the positions it
wrote; alt branches — the `if` inside an event — fork the environment
(a branch's guards never leak out; its writes invalidate for what
follows). An update the walk does not reach classifies
constraint-free.

The rules, per assignment occurrence:

* `x := c` — the constant joins the set;
* `x := v [± consts]`, `x := (at a e)` — an **edge** from the source
  unit, carrying the live restriction on `v` and the constant shift.
  A genuine shift can only diverge by feeding itself, so it is kept
  when any of three structural facts holds: a guard bounds its
  advancing side (`x := x + 1` under `(< x N)`), the destination has
  a declared window (every join clips to it), or the destination
  cannot reach back to the source through the edges (no cycle). On a
  cycle into an undeclared unit with no guard, the honest answer is
  top;
* `x := e % k` (k a positive constant) — the interval `[0, k)`,
  tagged `mod` (assumes the operand nonnegative, as an index is; the
  tag keeps the assumption visible);
* a boolean-valued rhs — the set `{0, 1}`;
* `havoc x lo hi` — `[lo, hi)` as a set when small, interval else;
* anything else — top: multi-variable arithmetic is *honestly*
  unconstrained here; this pass detects enumerated state and
  guard-bounded motion, not general arithmetic.

Then one fixpoint over the edges propagates: each edge joins its
source's domain restricted to the edge's window and shifted (exact on
sets — holes survive; a clamp on intervals), the join clipped to the
destination's **declared window**. It terminates with no cap: copies
invent no values, guarded shifts live inside their constant window
shifted, and the surviving unguarded shifts either land in a declared
window or sit on no cycle — every hull stays within a fixed finite
range.

The declared window is the semantic anchor: a write outside a
declared `[lo, hi)` is a run error, never a state, so stored values
cannot leave it — a unit whose members all declare is clipped to the
union of their declarations, "top" for it means its whole window, and
a declared byte stays a byte no matter what arithmetic feeds it.

Seeds contribute their values (a never-assigned unit ends as `frozen`:
its initial values are its whole life). No refinement by reachability —
guards restrict what an assignment can *read*, they never shrink a
unit's own domain; this is decoration, not verification. A set that
outgrows the cap degrades to its interval hull, visible as the kind.

What the data answers: the proportion of units statically bounded, the
size distribution of the small domains, and the **holes** — a set kept
exactly (never widened to a hull) exposes sentinel patterns like
`{0, 1, 2, 255}`, which an interval abstraction would silently paper
over.

## 6. Search order

FIFO expansion = breadth-first: shortest witnesses first, and the natural
seam for later strategies (guided walks, random simulation, DFS) — a
strategy owns the queue discipline, nothing else changes.
