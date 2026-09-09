# Handoff — budgets and time sharing (`include/hsc/sched/`)

Current state only, next action first. Design: `include/hsc/sched/algorithm.md`
(agreed direction: PetriSpot's task/slice/scheduler vocabulary, symbolic
tasks as continuations kept as data, one budget for time and memory).

## Engineering — next

1. **Tasks and slices** (`algorithm.md` §3, §3b, §7.1). Done: the stop
   state on the manager, partial results kept out of the caches, the
   surface orders `(budget S)` / `NAME partial` / `(reach … from)` /
   `(stock … since …)` (`examples/models/budget_hanoi.hsc` exercises the
   loop), PetriSpot's `sched/` vendored. `hsc --stdin` streams orders, one answer flushed per form. Next: the reachable set as a `Task`
   (`run(slice)` = budget, reach from, stock; slices growing geometrically;
   the report from the stock); a CTL property as a task.
2. **Set metrics between epochs** (§3b): extend `order/profile` with fan-in
   and fan-out per level, cardinal per subshape and growth against a
   previous set, the domain reached per leaf; `(profile)` prints them, the
   task reports them. No instrumentation inside saturation.
3. **The budget object**: time and memory, `remaining()`, polls only in
   loops that can exceed a second, deadlines exact to a second or two; the
   alarm behind a flag (armed first thing, done); `hsc-pn`'s phases on the
   budget; the accounting line. Margin is the job's (`sweep_job.sh`: 30 s).
4. **The shape portfolio in one process** after the sweep (`experiments/order/`).

## Blockers / open

* Memory accounting: the manager must report its footprint (tables plus
  caches) cheaply for the memory deadline; check what `mem/stats.hh` gives.
* The parse (SAX) cannot be sliced; it stays on the budget by being timed
  and by the alarm backstop only.
