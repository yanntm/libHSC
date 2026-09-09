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
  fields, all computed on the returned set (§3b), none inside the run —
  `steps`: rounds run; `claims`: answers produced (a FORMULA, a StateSpace
  value, a fixpoint closed); `novelty`: states gained since the previous
  slice (cardinal growth); `stalls`: subshapes that gained nothing;
  `heuristicDrop`: the whole gained nothing; `capped`, `micros`,
  `finished` verbatim — plus what we add: the memory footprint and the
  set's profile;
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

## 3b. Stopping a fixpoint correctly, and epochs

**What a stop must guarantee.** A forward closure interrupted anywhere holds a
set that is sound (every state in it is reachable) and incomplete; the
danger is not the set, it is the memo: a partial value entered in a cache
would be read later as exact. libDDD solved this with a global flag, single
thread: once raised, every operation unwinds — heavy loops end at their next
round, recursions return what they have — **without inserting into any
cache**, the root returns its set marked *partial*, and the flag is reset
only once the saturation is back at the root. The caches are then dropped,
garbage collected, and the next round restarts from that set. Ours is the
same discipline on the manager: a `stopping` flag raised by the budget (time
or memory) or by the coordinator; `check()` returns it; every closure loop
and every composite operation that sees it returns its current value and
skips the cache insert; the surface form returns `{set, partial}`. Entries
inserted before the flag rose are exact and may be kept; whether they are
kept or flushed is the memory budget's call, not correctness's.

**Epochs must grow.** Without its caches a saturation makes no progress, so
an interruption that comes too often kills a computation instead of helping
it. libDDD's rule: the interval between interruptions grows geometrically.
Ours the same: the first slice of a fixpoint task is short (the broad
experiment), each resumed slice is longer than the last, and a flush is
what a memory deadline does, not what an epoch does by default.

**What an epoch reads: the set, nothing inside the computation.** No
instrumented counting during saturation: the rewrites adapt the transition
relation to the shape, what is one event against a set of events is not
well defined once fused, and counting would cost. The report is computed
on the set the task returned, by the same routine whatever the shape:

* nodes and arcs per sort of the shape (`order/profile.hh`, already);
  total nodes; fan-in and fan-out per level;
* per reachable subshape, its cardinal, and against the previous epoch's
  set, which subshapes gained states — the subshapes that fired and found
  new states, which is the progress signal, and correlated with where the
  saturation stalls;
* the domain reached per leaf variable: the one metric comparable across
  shapes, since every shape has the same leaves;
* the cardinal and the node count of the whole, and the memory footprint.

That is enough for the coordinator's decisions (§4) and for the pages of
the sweep; time per saturation level, if ever wanted, is a later,
separate instrument.

**A divergence watch.** The leaf domains an epoch reads are also a signal
in themselves: a variable whose domain has grown far past its initial
tokens, and keeps growing epoch after epoch, is a candidate unbounded
place — a saturation that will diverge inside its tight loop if nobody
looks. The epoch should break out, report the variables going wild, and a
cheap confirmation can then settle it instead of spending the budget.

The confirmation is a **pumping pair**, not a coverability engine. For a
Petri net, a reachable `m` and a run `m →σ m'` with `m' ≥ m` componentwise
and `m'(p) > m(p)` prove `p` unbounded: by monotonicity `σ` fires again from
`m'`, and again. The partial set is sound (every state in it is reachable),
so: take the leaf `p` whose domain runs away, its largest value `X` seen,
and a shortest path from the initial state to a state with `p = X`
(`trace/path`, inside the partial set); scan that concrete run for a pair
`i < j` with `m_j ≥ m_i` and `m_j(p) > m_i(p)` — a run that reaches a large
`X` almost always contains the loop it pumped. Failing that, the run to
`p = X+1`, or a path from the states at `X` to those at `X+1` and the same
scan; failing that, a Parikh guide — a vector `x ≥ 0` with `C·x ≥ 0` and
`(C·x)_p > 0`, the state equation's own certificate of a pump, found by the
invariant machinery — handed to the walker as the sequence to realise from
a reachable state. A confirmed pump answers the StateSpace examination at
once: `MAX_TOKEN_IN_PLACE`, `MAX_TOKEN_PER_MARKING`, `STATES` and
`TRANSITIONS` are all `+inf`. DoubleExponent still explodes honestly, being
bounded; the unbounded nets stop being timeouts.

Prototyped: `(pump NAME [LEAF])` (`src/surface_cover.cc`), `hsc-pn --cover`;
the leaf theory breaks a closure out when a domain passes the divergence
limit (`support_algebra::note_divergence`), and every per-element or
per-arc loop that can run for seconds polls. CryptoMiner and FunctionPointer
answer `+inf` in 16 s of a 20 s budget; the 754-place BugTracking does not
within 5 s of pump budget — its seed neighbourhood is already expensive.

**What an epoch may do.** Test the known goals on the partial set (every
positive reachability claim is sound on it; negative claims and exact
counts wait for the fixpoint); flush under memory pressure and resume, or
park; hand the set to another shape when the top has not moved for
several epochs. Saturation's blind spot stays noted — events whose top
level is the root fire last, everything below being saturated first — as
a schedule variant an epoch could try, not as something to measure.

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

1. **Tasks and slices.** Done: PetriSpot's `sched/Task.h` and
   `Scheduler.h` vendored; the manager's stop state (a deadline or a
   request, `stopping()` at round boundaries, results marked partial and
   refused by the caches, reset at the root); the surface orders
   `(budget S)`, `NAME partial`, `(reach … from NAME)`, `(stock NAME [since
   OTHER])` — the epoch loop as a script anyone can drive, a stream of
   orders included. Next: the reachable set as a `Task` whose `run(slice)`
   is that loop with geometrically growing slices; a CTL property as the
   second task.
2. **Set metrics between epochs.** `order/profile` extended with fan-in and
   fan-out per level, cardinal per subshape and its growth against the
   previous set, the domain reached per leaf; printed by `(profile)` and by
   the task's report.
3. **The budget object** (time and memory, `remaining()`, coarse polls in
   loops over a second), the alarm behind a flag, phases of `hsc-pn` on it,
   the per-phase accounting line.
4. **The coordinator for the shape portfolio in one process**, after the
   sweep says which shapes belong.

## 8. What this asks of PetriSpot

`Task`, `Slice`, `SliceReport`, `Scheduler` are already generic in
`Petri/src/walk/`; the symbolic side needs from them: a memory field in the
report (or a side channel the coordinator reads), the notion that a task's
steps are coarse (already there as `capped`), and a home for the types that
is not "walk" — a `sched/` folder and namespace, vendored here as
`petri/` is. The coordinator's tables (yield per kind, per (state, tool))
stay PetriSpot's design; ours adds shapes as kinds and memory as a budget.
Recorded on PetriSpot's side in `PORTFOLIO.md`.
