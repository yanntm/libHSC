# Budgets and time sharing for symbolic tasks — the design

## 1. What went wrong, stated generally

`hsc-pn --totalTime` is a SIGALRM: a handler prints the open UNKNOWNs and
`_exit`s. Inside the calculus there is a cooperative hook
(`manager::set_interrupt`, `check_interrupt()` throwing `interrupted`),
polled at the heads of the closure loops, set by the CTL fair-share
scheduler to a per-property deadline. Nothing between the two: the parse,
the shape heuristics, the invariant calculator, the counts, the emission
never look at the clock, and inside one closure iteration the overshoot is
whatever that iteration costs. The signal papers over all of it, and a
signal is a poor foundation: it knows nothing of phases, it cannot say how
much is left, it exits from a handler with whatever was on stdout, and its
delivery is not ours to guarantee. In the first sweep 21 runs of 1490
outlived it by two seconds and were killed from outside with nothing said.

The systemic answer is not a better signal. It is that every piece of work
draws on **one budget**, polls it **cheaply and everywhere**, and is
**resumable**, so that a run always ends by its own decision with its
answers printed and its accounting done — and so that several pieces of
work can share the cores of one process under one budget, which is what a
portfolio is.

## 2. The budget

One object per manager: a steady-clock deadline (or none), `remaining()`,
and `check()`. Granularity is deliberately coarse: our outside budgets are
hundreds to thousands of seconds, never hundreds of milliseconds, so a
deadline honoured within one or two seconds is exact enough, and polling
must not become a line in the profile. The rule is therefore: **a loop that
can run for more than a second polls, nothing finer does.** That is the
closure schedules at every level (a round of the saturation loop, a gfp
round, an image of a step term), the count loops, the heuristics'
iterations; not the canonicalizer, not the leaf operations, not the cache.
Where a single iteration can itself exceed a second (one image over a very
large set), the poll sits one level down, at the node visit of the top
sort, amortised by a counter so the clock is read rarely. When the deadline
is past, `check()` throws `interrupted`; the exception unwinds to the task
boundary (§3), never further than the surface form being run. The surface's
`set_interrupt` becomes `set_deadline`; the alarm stays in the tool behind a
flag as a backstop, armed at the first line of `main`, with a margin that
the job, not the tool, decides.

Budgets are hierarchical the way PetriSpot's are: a phase or a child task
receives a deadline no later than its parent's, computed from the parent's
`remaining()` and a share; what it does not use returns to the parent by
construction. The CTL fair-share loop already does this by hand; it becomes
an instance.

**Memory is a budget too.** The nodes cap a one-core job at 6 GB; the
manager knows its footprint (the tables' sizes, the caches' sizes). A
memory deadline is polled at the same points: past it, the task is parked
(§4), its caches dropped, and the coordinator decides whether a smaller
configuration takes over. This is the one dimension explicit walkers rarely
meet and symbolic ones always do; it is a first-class field of the report.

## 3. A symbolic task is a continuation kept as data

PetriSpot's insight is that a walk between two steps is heap data, so a task
is an object whose `run(slice)` returns at a step boundary and resumes later
by a call. A symbolic fixpoint is the same, one level up: between two
iterations its whole state is the current set (a code) and the memo — the
unique table and the operation caches, which survive the interruption and
are what makes the resumed run replay its prefix at cache speed. The CTL
checker already relies on this ("a property stopped by its deadline resumes
from what it memoised"). So:

* a **step** is one iteration of the closure schedule at the task's top
  level (one round of the saturation loop, one round of a gfp, one image of
  a step term, one selection of a count loop). Steps are coarse and uneven;
  the slice's wall-clock cap is the watchdog, as in PetriSpot's *coarse*
  tasks, and the amortised `check()` inside a step is what makes the cap
  hold within milliseconds;
* a **slice** is `{steps, deadline}` verbatim (`walk/Slice`): the task sets
  the manager's deadline to the slice's, runs until `steps` iterations or
  `interrupted`, and returns;
* a **report** is PetriSpot's `SliceReport` with the symbolic reading of its
  fields — `steps`: iterations run; `claims`: answers produced (a FORMULA,
  a StateSpace value, a fixpoint closed); `novelty`: nodes created in the
  slice (the diagram grew: the closure is still discovering); `stalls`:
  levels whose events all fired to no effect; `heuristicDrop`: the cardinal
  or the node count stopped growing over the slice; `capped`, `micros`,
  `finished` verbatim — plus what we add: the memory footprint after the
  slice and the per-level progress of §3b;
* the **task kinds** of `hsc-pn`: the reachable set under a shape; a CTL
  property (the checker's node evaluation, already resumable); the
  StateSpace counts (per transition, a loop of selections); a shape
  heuristic (FORCE and Louvain iterate and keep the best seen, the
  invariant calculator takes a deadline); a witness search (layers). The
  parse and the emission are not tasks: they run once, on the budget, and
  are the only phases that cannot be sliced.

A task not running holds its set and its memo. Parking a task (§4) drops
the caches and keeps the set and the unique table: the cheap part of a
resume is lost, the work is not.

## 3b. Epochs: the interruption as a rhythm, not only a stop

ITS-Tools had this in one form: an interrupt on memory pressure
(`shouldGarbage`) that forced the saturation to walk back up to the root,
so that the caches could be flushed almost entirely, and the properties
tested on the set reached so far. The same mechanism, taken as a rhythm,
is what turns a long budget into insight rather than one long wait. An
**epoch** ends when the slice ends: the closure unwinds to the root and the
task returns its set and a report; between epochs the coordinator can

* **test what is known on the partial set.** A reached set that is not yet
  the fixpoint is sound for every positive reachability claim (a state in it
  is reachable): reachability goals close, bounds rise, `EF` witnesses
  exist; only the negative claims and the exact counts wait for the
  fixpoint. Facts close early, and their goals leave the budget;
* **flush under memory pressure** — the caches go, the unique table and the
  set stay — and resume, or park the task and let a smaller configuration
  take over;
* **read what progressed.** The saturation schedule knows, per level and
  per event, whether the last round changed anything: which events fired,
  which levels are stable, how the cardinal and the node count moved since
  the last epoch. That is the report's `novelty` and `heuristicDrop`
  refined to the level, and it is the introspection the coordinator
  reasons on: a shape whose top levels never move is stalled, whatever its
  cardinal says;
* **change the schedule.** Saturation has a known blind spot: an event
  whose top level is the root is very hard to ever fire, because everything
  below must be saturated first. An epoch can open with one round of the
  events of high top level before the schedule restarts, or reorder the
  schedule by what stalled, or hand the set to another shape (the flat
  order for a counter net once the hierarchical one shows no progress at
  the top). None of this is possible from inside one uninterrupted
  fixpoint.

So the interruption serves three masters — the deadline, the memory, and
the coordinator's curiosity — and the third is the one that matters on the
hard instances: an hour of budget spent as sixty epochs, each read, is how a
model's mechanics get understood and its heuristics fitted while the run is
still going.

## 4. Scheduler and coordinator, unchanged

The scheduler is PetriSpot's, vendored: one queue on virtual time, `N`
runners, one lock, a task's clock advancing by slice time over share. It
decides nothing. The coordinator reads reports and decides: continue a task
that progressed (claims, novelty, a fixpoint closed), park one that did not
over a window (its budget returns, its caches are dropped), spawn from a
catalogue when a runner would idle and the live count is under a cap.
Shares move toward productive kinds with a floor and a decay; diversity
first, on every model, until this model says which kind pays.

The symbolic distinctions the coordinator needs to know: tasks are few and
heavy (a handful of shapes, not ten thousand subquests); the shared memory
is the manager's tables, so tasks of one manager cooperate on the unique
table and compete for the 6 GB; and a shape configuration is a *kind*,
so the portfolio over shapes that the driver runs today as four processes
becomes four tasks of one process, with the memory accounted once and the
losing shapes parked instead of killed.

## 5. Meshing with the explicit side

Same `Task`, same `Scheduler`, same reports, same budget arithmetic: an
`hsc` task can be handed to a PetriSpot scheduler in one process, or a
PetriSpot walker to ours. What flows between them is facts, in PetriSpot's
pool sense (`PORTFOLIO.md`: an item is goals, model, budget; effort is
reported per item): a symbolic task closes a goal (a FORMULA, a bound, a
count) as a fact; a walker's best marking is a hint a symbolic task can
seed a backward search from; a symbolic reachable set, when it exists, is
the oracle every walker's claim is checked against and the place the
coordinator reads which targets are unreachable and stops spending on.
None of this needs a framework beyond the shared types and the pool.

## 6. The accounting a run prints

Under `-v`, one line per phase — parse, shape, `R`, counts, properties —
with the budget spent and the memory after it, the time left at each
answer (`hsc-pn: answered <name> at <s>`, already there), and at the end
what was left unspent. This is the record the sweep pages read, and the
input the coordinator will learn from: yield per kind per second, on this
model.

## 7. Order of work

0. The saturation schedule reports progress per level and per event
   (which fired, which stalled, cardinal and nodes since the last epoch),
   and can be asked to unwind at a round boundary — the instrument every
   later step reads.

1. The budget object in the manager, the amortised `check()` on the hot
   paths, `set_deadline` in the surface; the alarm behind a flag, armed
   first thing; margins in the jobs, not in the tool.
2. The phases of `hsc-pn` on the budget: the invariant calculator given
   `remaining()`, FORCE and Louvain iterations polling, the StateSpace
   count loop polling and printing what it has, the emission timed.
3. Vendor `walk/Task.h` and `walk/Scheduler.h`; the symbolic task kinds;
   the reachable set as a task; the CTL fair share re-expressed as tasks.
4. The coordinator for the shape portfolio in one process, once the sweep
   says which shapes belong in it.
