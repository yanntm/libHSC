# Shape-directed conjunction of linear selectors

Status: design for review and implementation. This document specifies the
specialised evaluator first; automatic selection by the general expression
compiler is a later integration step.

## 1. Operation and scope

Given a finite diagram X and a conjunction C of linear constraints over its
frontier positions, compute exactly

    filter(X, C) = {m in X : every c in C holds on m}.

Each constraint has a sparse coefficient map and is an equality or inequality:

    sum(a[p] * m[p]) = k       or       sum(a[p] * m[p]) <= k.

Coefficients may have either sign. A lower bound is represented by reversing
signs with checked arithmetic. Positions refer to occurrences in the frontier,
not merely to a reusable leaf sort. Zero coefficients do not belong to the
support. Constant constraints are decided before scheduling.

The operation is a filter of X. It neither adds paths nor reconstructs X from
independent coordinate domains. It applies equally to a box, a previously
restricted invariant set, and another finite diagram. For the dead-transition
client, X must already contain every relevant reachable marking and the
constraints must be justified invariants or bounds. The evaluator itself
proves no fact about reachability.

The first API exposes this operation directly: input diagram, shape/coordinate
context, a collection of sparse linear constraints, and an absolute deadline.
It returns a completed exact diagram or signals interruption/failure. An exact
empty result must be distinguishable from an unfinished computation. Concrete
C++ names and ownership details follow the implementation; there is no new
public declaration yet.

## 2. Why the schedule is legal

For a constraint c, write its selector as S_c(X) = X intersect [[c]]. Then

    S_a composed with S_b = S_b composed with S_a,
    S_c composed with S_c = S_c.

Every selector only removes paths. A path retained after one selector continues
to satisfy it after any other selector has run. Consequently, applying every
selector once, in any order, computes the conjunction and its common fixed
point below X. No fixed-point iteration is necessary.

This is analogous to one descending saturation pass with contracting effects:
finish the work belonging below a shape cut, then perform the work that needs
both sides of that cut. The algebraic justification is commutation and
idempotence of pure selectors. Path-linearity alone does not grant this freedom
to arbitrary updates or event compositions.

Commutativity allows scheduling by support rather than by textual operand
order. Flattening and canonicalising an AND is useful for sharing, but sorting
expression handles is not the scheduling algorithm.

## 3. Attach each constraint to its application level

Use the existing binary shape. At a node with head H and tail T, partition the
pending constraints into three groups:

* H-local: their entire support lies in H;
* T-local: their entire support lies in T;
* crossing: their support intersects both H and T.

Pass each local group recursively to its side, rebasing positions consistently.
Keep the crossing group at this node. A constraint is attached to the lowest
shape node containing its whole support. A single-position constraint reaches
a leaf and becomes a local restriction there.

Build this schedule once for a fixed shape and constraint collection, using
sparse supports and the shape's coordinate mapping. Equal subshapes at different
frontier offsets are not interchangeable coordinate contexts. No dense
per-position constraint table is required.

At one application level, process crossing constraints in a deterministic
order. They commute, so changing that order affects cost but not the answer.
The first implementation need not combine several crossing constraints into a
joint vector of residual sums. Such fusion can multiply the residual-state
space and is a separate optimisation to measure.

## 4. Descend, restrict, then apply crossing selectors

At a composite node, a diagram represents a union of arcs P x Q, where P is a
head diagram and Q is a tail diagram. Evaluate the scheduled local groups on
their corresponding arc components. Discard empty components, and reconstruct
the restricted diagram through the usual canonicalisation machinery. Apply
the crossing selectors to that result, one after another.

Conceptually:

    restrict(X, schedule at H x T):
        Y = empty
        for each arc (P, Q) of X:
            P' = restrict(P, schedule for H)
            Q' = restrict(Q, schedule for T)
            add P' x Q' to Y when both are nonempty
        canonicalise Y
        for each crossing constraint c scheduled here:
            Y = knapsack_filter(Y, c)
        return Y

At a leaf, intersect its existing value set with its scheduled local tests.
An empty schedule returns its input immediately. Shared subdiagrams reuse the
memoised result for the same schedule and coordinate context.

Local filtering can make formerly disjoint primes overlap: their associated
subs must then be joined by canonicalisation. Merely rewriting the arc array
without restoring the diagram invariants is insufficient.

“One pass” means one scheduled application of each constraint at its level.
It does not mean that each physical diagram node is visited exactly once:
sharing, different residual sums, and successive crossing selectors may cause
several distinct memoised computations on the same node.

## 5. Knapsack on the current diagram

The crossing evaluator must filter the diagram produced by the recursive
work. Constructing an independent equality diagram over the original box and
intersecting it afterwards defeats the purpose of this schedule.

For an equality at a node, split its weighted sum between the two sides. For
each existing arc P x Q, partition the represented head paths by their realised
contribution h. Recursively retain from Q exactly the paths whose tail
contribution is k - h. Reconstruct the union of the retained rectangles. For
an upper bound, the residual tail bound is k - h instead of a required exact
sum. At a leaf, the restriction is a comparison on its existing values.

The contribution partitions are diagrams, not enumerations of complete paths.
Their construction follows the shape recursively and memoises by the input
subdiagram and the relevant sparse coefficient slice. A weighted-sum class
contains exactly the input paths realising that sum. Different realisations
with the same contribution share a class.

Crucially, work stays attached to an existing arc. Joining all head projections
and all tail projections independently would admit combinations that were not
in X. Filtering preserves the correlation between each prime and its sub.

Subtrees outside a constraint's support have zero contribution and are reused.
Safe lower and upper contribution bounds permit early rejection or acceptance
of a residual problem. Bounds derived from actual subdiagrams can be tighter
than coordinate-domain bounds; either is sound if it encloses every represented
contribution. Bounds must account for signed coefficients. Arithmetic overflow
must be detected rather than wrap into a false proof; unsupported arithmetic
aborts the filtering attempt, leaving the caller's input valid.

This is the existing knapsack idea adapted to an already restricted diagram.
The implementation may reuse its arithmetic and pruning, but not its assumption
that independent leaf domains define the input set.

## 6. Sharing and execution context

There are two distinct memoisation needs:

* scheduled restriction: input diagram, schedule identity, and the
  shape/coordinate context needed to interpret its supports;
* knapsack restriction: input diagram, constraint or coefficient-slice
  identity, relation, residual target/bound, and coordinate context.

A contribution-partition cache also depends on the input diagram and coefficient
slice. A diagram handle alone is insufficient: the same shared node can be
visited under different constraints or residual targets.

Normalise constraint collections independently of textual nesting and order:
flatten conjunctions, combine repeated coefficient positions, remove zeros,
and deduplicate identical constraints. This exposes equal schedules to sharing.
It does not require algebraically minimising the constraint system.

All state belongs to an evaluator invocation or an explicitly scoped manager
context. No global caches. Cache lifetimes must respect diagram ownership and
any manager epoch or reclamation boundary; the first implementation need not
reuse caches across invocations or shape changes.

## 7. Deadline and sound interruption

The reduction client supplies one absolute deadline for the whole preparation.
Flow computation, shape/constraint preparation, invariant-diagram construction,
and dead-transition testing draw from that deadline. Starting a fresh full
budget after constructing the invariant set is not permitted.

The evaluator checks before beginning work and polls inside potentially long
operations: contribution partitioning, residual-sum loops, reconstruction and
set operations. Checking only between constraints leaves a single expensive
constraint unbounded. Use the manager's existing cooperative interruption
mechanism, with amortised polling on hot paths.

The initial contract is transactional with respect to the returned diagram:
only a fully completed conjunction returns a result. If interrupted, discard
the unfinished result and preserve the original X. An incomplete construction
can omit satisfying paths; treating it as an invariant over-approximation could
prove a live transition dead. An unfinished result is never the empty-set
answer.

Later, a completed prefix of selectors could be returned with an explicit
weaker-result contract. That optimisation is unnecessary initially. Existing
confirmed dead-transition proofs remain valid, but no new proof may depend on
an unfinished invariant set. A reduction-budget interruption returns control
to the solving pipeline; it must not consume the examination's entire budget.

## 8. Correctness obligations

The proof is structural. At a leaf the result is exact by the local predicate.
At a composite node, selectors wholly on one side distribute over each arc's
Cartesian product and over the union of arcs. The recursive results therefore
apply exactly all constraints below this cut while preserving correlations.
The knapsack partitions one side by its exact contribution and applies the
corresponding residual comparison to the associated other side; its retained
rectangles are exactly the paths satisfying the crossing constraint.

Composition with each crossing selector gives precisely the conjunction at
this node. Induction yields filter(X, C) at the root. Reordering these completed
selectors preserves that answer by Section 2. Canonicalisation and memoisation
change representation and repeated work, not the represented set.

Termination within a budget additionally depends on polls in the operations
called by these recursions. The semantic argument alone is not a deadline
guarantee. Aborting safely is part of the API contract, not an exceptional
permission to use partial data.

## 9. Integration and evaluation

First expose the specialised operation to the invariant-set client. Supply the
box or previously established approximation and all retained constraints in
one call. Keep flow discovery, projection policy and dead-transition logic
outside this evaluator. Compare the completed result with the existing
construction where both finish, using diagram equality in a common manager or
another exact set comparison; equal cardinalities alone are insufficient.

Use real models, starting with a smaller Anderson instance and then
Anderson-PT-07. Keep model, shape, constraint collection and binary provenance
explicit so a changed constraint set is not mistaken for an evaluator speedup.
Separate flow time, diagram-construction time and dead-test time. Record peak
nodes/memory and work counters as well as represented cardinalities: a huge
represented set can have a small diagram. Check completed StateSpace values
against the existing oracle when exercising the full pipeline. Every diagnostic
example has a short external bound; a timeout is a finding, not a reason to
wait indefinitely. No cluster run is needed for this development step.

The performance claim to evaluate is that local restrictions reduce the work
seen by higher crossing constraints and avoid independent whole-box equality
diagrams. It is not a universal size or runtime bound: restricting a set can
increase its diagram size, and overlapping constraints at one cut can remain
expensive. The shared deadline must remain effective in those cases.

Only after this path is understood should the general compiler recognise a
conjunction of supported pure selectors and lower it to the same evaluator.
That rewrite must preserve the expression's coordinate mapping and arithmetic
semantics and leave unsupported predicates on the existing execution path.
It must not assume that arbitrary event compositions commute.
