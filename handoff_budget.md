# Handoff — budgets and time sharing (`include/hsc/sched/`)

Current state only, next action first. Design: `include/hsc/sched/algorithm.md`
(agreed direction: PetriSpot's task/slice/scheduler vocabulary, symbolic
tasks as continuations kept as data, one budget for time and memory).

## Engineering — next

0. **Progress introspection in the saturation schedule** (`algorithm.md`
   §3b): per level and per event, fired or stalled in the last round;
   cardinal and node count at epoch boundaries; an unwind at a round
   boundary on request. Polling only in loops that can exceed a second,
   deadlines exact to a second or two, nothing in the profile.
1. **The budget object** replaces the bool interrupt hook in `core/manager`:
   deadline, `remaining()`, amortised `check()` (counter, clock every few
   thousand calls). Poll it in the canonicalizer and the cache miss path,
   keep the existing closure-loop polls. Surface: `set_deadline`. Tool:
   the alarm behind a flag, armed at the first line of `main` (built,
   uncommitted at the time of writing: commit it). Margin is the job's:
   `sweep_job.sh` gives `--totalTime` 30 s under its `timeout`.
2. **Phases on the budget** in `hsc-pn`: invariant calculator with
   `remaining()`; FORCE and Louvain iterations polling; the StateSpace count
   loop polling and printing the values it has; the emission timed; the
   per-phase accounting line under `-v`.
3. **Vendor** `walk/Task.h`, `walk/Scheduler.h` (a `sched/vendor.sh` like
   `petri/vendor.sh`); write `sched/tasks.hh`: the reachable set and a CTL
   property as tasks with the symbolic reading of `SliceReport`.
4. **The shape portfolio in one process**: four shape kinds as tasks under
   one budget and one memory cap, parking instead of killing — after the
   sweep (`experiments/order/`) says which shapes belong.

## Blockers / open

* Memory accounting: the manager must report its footprint (tables plus
  caches) cheaply for the memory deadline; check what `mem/stats.hh` gives.
* The parse (SAX) cannot be sliced; it stays on the budget by being timed
  and by the alarm backstop only.
