# `sched/` — budgets, tasks and time sharing for symbolic work

Doc-first: this folder holds the design (`algorithm.md`) before the code.
It answers one question the first cluster sweep raised — how a run knows
how much time it has left, everywhere, and stops by itself — and it does so
in PetriSpot's vocabulary, because the two engines are meant to share one
process one day: explicit walkers and symbolic fixpoints as tasks of one
scheduler, under one budget, feeding one pool of facts.

## Position

Multi-core is not parallel loops. It is many heuristics thrown at a problem
that humans posed, time going to the ones that produce, measured rather than
guessed. PetriSpot built that for explicit exploration (`walk/Task.h`,
`walk/Scheduler.h`, `WALK_PLAN.md` §10.11–10.12, `PORTFOLIO.md`): tasks as
continuations kept as data, slices in steps with a wall-clock cap, a
CFS-style scheduler on virtual time, shares that follow results, a
coordinator that continues, parks or spawns, budgets that conserve down a
spawn tree, and a pool of facts and states as the shared memory. The
symbolic side has the same needs and one strong asset — an interrupted
fixpoint loses nothing, its memo tables *are* its continuation — so the
same objects fit with a few distinctions, spelled out in `algorithm.md`.

## What goes where

| piece | where | status |
|---|---|---|
| the budget: one deadline, `remaining()`, an amortised `check()` | `core/manager` (replaces the bool interrupt hook) | to build |
| polling points: closure loops (there), canonisation and cache misses (to add), the tool's phases (to add) | `core/`, `tools/` | to build |
| `Slice`, `SliceReport`, `Task`, `Scheduler` | vendored from PetriSpot `Petri/src/sched/` (`Task.h`, `Scheduler.h`, namespace `petri::sched`) by `vendor.sh`, the include path the only edit | vendored |
| symbolic tasks: reachable set, a CTL property, a shape heuristic, a count | `sched/tasks.hh` | to build |
| the coordinator policy for a symbolic portfolio (shares, parking, memory) | `sched/coordinator.hh` | after the sweep |
| the accounting line: budget spent per phase, time left at each answer | `hsc-pn -v` | to build |

## Rules kept from PetriSpot

Effort is core-seconds per item, reported back. A worker never blocks the
pool. Budgets conserve: a child's budget comes out of its parent's. The
scheduler keeps the queue and the runners and decides nothing; the
coordinator decides and owns the tables; a task explores and reports.
