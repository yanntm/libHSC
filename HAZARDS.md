# Hazards

Known-unsafe interactions, and what must happen before each is armed. This
file exists because a fast-moving prototype accumulates loaded guns faster
than it accumulates the discipline to unload them, and an undocumented one is
indistinguishable from a bug waiting for a workload big enough to find it.

Each entry states: **what breaks**, **what currently prevents it**, and **what
must be done before the preventing condition is removed**. An entry leaves
this file when it is fixed, not when it stops being scary.

---

## H1 — Collection would poison every cache. *(loaded, not armed)*

**What breaks.** `mem::intern::collect()` frees unmarked ids and bumps their
generation. Three structures cite ids raw and would keep citing them:

* `diagram_engine::ops_` — the operation cache. Entries hold `binary_op`
  operands and results as bare codes. After a collection, an entry keyed on a
  freed node answers with a code that now means something else. **Silently
  wrong answers**, not a crash.
* `diagram_engine::cardinal_memo_` — a `vector<double>` indexed by node code.
  A recycled id inherits the previous occupant's state count.
* Every `code` a caller is holding — examples hold `core::code` locals across
  operations.

**What prevents it now.** Nothing in `core/` calls `ref`, `deref` or
`collect`; `intern::collect` is exercised only by `tests/test_intern.cc` on
its own tables. Memory therefore grows monotonically and no citation is ever
stale. Every example fits in memory comfortably, so this has never been felt.

**Before wiring collection**, all of:

1. Roots must exist. `mem::strong<T>` is written and unused; callers holding
   diagrams across a collection must hold strong handles, or the mark phase
   has nothing to start from.
2. `ops_` must either be cleared at collection (correct, conservative, and
   exactly the wipe-all that G5 exists to escape) or hold `certificate`s and
   validate on hit. `mem::certificate` is written and unused — it was built
   for this and has never been asked to do it.
3. `cardinal_memo_` must be cleared, or become a proper cache. A side table
   indexed by code is the least defensible structure in the codebase.
4. The `mark_children` callback must reach *across* tables: a node cites leaf
   codes in a theory's table and prime codes in its own. `intern::collect`
   takes the callback precisely so the owner can do this, and no owner does.

**Why it is not done yet.** Doing it properly is the retention-policy work,
and doing it improperly (wipe-all) would bake in the behaviour the reboot
exists to avoid. It needs a workload that actually exhausts memory to be
designed against, and we do not have one.

---

## H2 — `int_set::shift` is unbounded. *(live, unexercised)*

**What breaks.** `shift(delta)` is the pushforward of `x ↦ x + delta`, with
no domain. Under a closure — `term_closure` iterates to a fixed point — a
non-zero shift never converges. **Infinite loop**, no diagnostic.

**What prevents it now.** No example uses `shift`. Hanoi assigns, the
philosophers only guard.

**Before using it.** The theory needs a declared domain per leaf so the
pushforward can clamp or the guard can bound. That is a real modelling
question (a Petri place is bounded by the net's invariants, not by
declaration) and it is the first thing a Petri importer will hit: `m -= w` is
a shift, and `(m -= w)*` on an unbounded place diverges. In practice the
guard `m >= w` bounds it from below, so the loop terminates — but by
accident, not by construction, and nothing checks.

---

## H3 — The accumulator scans linearly. *(live, bounded by workload)*

**What breaks.** `diagram_engine::accumulator::add` finds the entry for a sub
by walking the vector. At arity *k* the regroup is O(k²). Nothing is wrong;
it just stops scaling.

**What prevents it now.** Node arities in the examples are ≤ 16. The
canonization sieve is O(k²) in *meets* anyway, so the scan is not the leading
term at these sizes.

**Before it matters.** A real model with wide nodes — a place with a large
marking domain, an array leaf — changes the leading term. The fix is a small
sorted index or a hash side table, and it should be made because a
measurement says so, not because this file says it might.

---

## H4 — Terms with no path from a test. *(closed for `compose`)*

Kept as a record: `op_kind::compose` shipped with `core: operations` and
nothing used it until the differential test was written. Any term kind that
no example reaches is a term kind whose semantics are a guess.

**Standing rule this suggests.** A constructor in `op_table` with no path
from an example or a differential test is not a feature, it is an untested
assertion about what we meant.

---

## H5 — Cache entries pin nothing, and the cache is unbounded in practice.

**What breaks.** `mem::cache` has a fixed capacity and evicts by LRU, so it
is bounded. But `diagram_engine` constructs it with `1 << 20` entries and
never revisits that number, and the *intern tables* it protects are unbounded.
A model that outgrows memory will do so in `nodes_`, where there is no
policy at all.

**What prevents it now.** Nothing. It simply has not happened yet.

**Before it matters.** Same prerequisite as H1: a workload that exhausts
memory. The two are one piece of work.
