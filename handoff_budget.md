# Handoff — budgets and time sharing (`include/hsc/sched/`)

Current state only, next action first. Design: `include/hsc/sched/algorithm.md`
(agreed direction: PetriSpot's task/slice/scheduler vocabulary, symbolic
tasks as continuations kept as data, one budget for time and memory).

## Engineering — next

1. **Tasks and slices** (`algorithm.md` §3, §3b, §7.1): the `stopping` flag
   on the manager — raised by budget or coordinator, read by every closure
   loop at its round boundary, no cache insert once raised, `{set, partial}`
   returned at the surface, reset at the root; vendor `walk/Task.h` and
   `walk/Scheduler.h` once PetriSpot has given them their `sched/` home
   (§8); the reachable set as a task with geometrically growing slices; a
   CTL property as a task.
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
