# `trace/` — paths and witnesses over diagrams

Runnable evidence for a symbolic verdict: a **path** from a source set to a
target set through the reachability graph, as a sequence of states (word
literals) and event names, and on top of it the **witness tree** of a CTL
verdict — the forward form's set expressions read back as paths. A
transverse concern kept in its own package: nothing in `core/`, `ctl/` or
the surface changes for it; it only *reads* diagrams and terms.

## Placement

Above `core/` and beside `ctl/`: depends on `core/` (diagrams, terms, the
inverter for one backward step) and on `ctl/` for the witness tree (the
question tree and its set expressions). Knows nothing about the surface;
the surface binds `(path …)` and `(witness …)` to it and prints the words in
its own syntax.

## Files

* `algorithm.md` — the layered search, the backtrack through inverted
  events, the witness tree per set-expression kind.
* `path.hh` — `path(mgr, model, from, to, constraint)`: the states and event
  names of a shortest path, or none.
* `witness.hh` — the witness tree of a `ctl` verdict (later).

Sources: `src/trace/`. Tests: the self-checking `examples/models/trace_*.hsc`
(a printed path replayed by the explicit engine reaches its target).
