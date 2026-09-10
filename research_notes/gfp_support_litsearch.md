# Literature search prompt — the greatest fixpoint of support over an invariant set

The prompt below is meant for a web-equipped assistant. Its answers feed
`~/git/Library`; nothing is cited in our papers before we have read it.

---

I am working on symbolic (decision-diagram) reachability analysis of Petri
nets and I would like a literature search on the following idea, which we
have implemented and measured but not yet placed in the literature.

**Setting.** A place/transition Petri net with initial marking m0, its
reachable set R. Decision diagrams (hierarchical set decision diagrams,
MDD-like) represent sets of markings; the transition relation is applied as
an image operator post(X) (successors) and pre(X) (predecessors), and the
classic construction of R is the least fixpoint R = lfp X ↦ {m0} ∪ post(X),
computed by breadth-first iteration or by saturation (Ciardo et al.).

**The idea.** Instead of climbing to R from below, start from an
over-approximation S ⊇ R given in closed form by linear invariants and
descend:

1. S is the set of *integer* markings satisfying every P-flow equality
   (Σ f_p·m(p) = Σ f_p·m0(p) for each flow f of the left kernel of the
   incidence matrix), the bounds those flows imply, structural facts
   (places never marked, unit-safety and per-unit mutual exclusion tags of
   nested-unit Petri nets), all built directly as a decision diagram (a
   knapsack along the variable order), not by a solver query per question.
2. Refine S by the greatest fixpoint of the *support* operator:
   G = gfp X ↦ X ∩ ({m0} ∪ post(X)) below S — the largest subset of S in
   which every marking is initial or has a predecessor inside the set.
   Since R satisfies that condition, R ⊆ G ⊆ S. Each round is one forward
   image of an S-sized set; the iteration removes the markings no chain of
   firings inside S supports from m0. Optionally intersect also with the
   backward condition X ∩ (pre(X) ∪ Dead), with deadlocks counted as their
   own successors, so R still satisfies it.
3. Use G (or S) for impossibility: a goal disjoint from G is unreachable;
   a transition enabled by no marking of G is dead; a target's backward
   closure pre*(X) ∩ S decides reachability exactly (m0 in it iff
   reachable), and the one-step version pre(X) ∩ (S ∖ X) = ∅ with m0 ∉ X
   is a cheap k-induction-like refutation, computed on the diagram and
   shared across all transitions of a net.

**What we observe.** On some nets G = R exactly (an aircraft landing
controller model, 89 places: S has 6·10^14 markings, 12 rounds of the gfp
give the 43 463 reachable markings). G differs from R exactly by the
markings of S that lie on or downstream of a cycle of spurious markings
(an infinite backward path inside S), so exactness depends on the garbage
of S being acyclic under reverse firing. On a net with 27 370 transitions
of which 24 601 are structurally dead and 854 more are killed by the
one-step test, one image of S under the 1915 surviving transitions takes
about 5 s where the image under all transitions did not return in 385 s.

**What I would like to know.**

1. Is the descending iteration "greatest fixpoint of X ↦ X ∩ (init ∪
   post(X)) below an invariant-defined set" known, under any name, in
   symbolic model checking or Petri net analysis? Candidate names:
   support, co-reachability pruning, backward-supported states, "reach
   from init within an invariant", forward-backward refinement.
2. The forward/backward combination in abstract interpretation (Cousot &
   Cousot, "Refining model checking by abstract interpretation", 1999, and
   the earlier forward-backward analyses) — has it been instantiated with
   an exact set representation (BDD/MDD) over a Petri net's marking space,
   starting from the state-equation or P-invariant set?
3. Petri net coverability pruned by over-approximations: Blondin, Finkel,
   Haase, Haddad ("Approaching the coverability problem continuously",
   TACAS 2016), Geffroy, Leroux, Sutre ("Occam's razor applied to the Petri
   net coverability problem", 2016/2018), Kloos, Majumdar, Niksic, Piskac
   ("Incremental, inductive coverability", CAV 2013), Esparza, Ledesma-
   Garza, Majumdar, Meyer, Niksic ("An SMT-based approach to coverability
   analysis", CAV 2014), Esparza & Melzer ("Verification of safety
   properties using integer programming: beyond the state equation", 2000).
   Which of these hold the over-approximation as an explicit set and
   iterate a set operator on it, rather than querying an LP/SMT oracle?
4. The characterisation "G = R ∪ {markings with an infinite backward path
   inside S}": is there a known result on when the state-equation (or
   P-invariant) over-approximation has acyclic garbage, or on classes of
   nets where invariants plus this gfp are exact? Related: the trap and
   siphon refinements of the state equation, and "structurally
   inductive" characterisations.
5. Using P-invariants to constrain the *encoding* of a BDD/MDD state
   space (Pastor, Cortadella, Roig et al., "Structural methods to improve
   the symbolic analysis of Petri nets", ICATPN 1999, and follow-ups):
   did any of that line use the invariant set as a starting point for a
   descending fixpoint, or only as a domain restriction of the lfp?
6. IC3/PDR and k-induction (Bradley 2011; Sheeran, Singh, Stålmarck 2000):
   any work that strengthens an inductive frame by removing *all*
   unsupported states at once with a set representation, rather than one
   counterexample-to-induction clause at a time?
7. Greatest-fixpoint computations over decision diagrams with a
   saturation-style (locality-driven, chaotic) schedule: does one exist for
   descending iterations, as saturation exists for ascending ones (Ciardo,
   Lüttgen, Siminiceanu, and later work)? Anything from the LTL/CTL side
   (EG hulls, "trim" operators of Emerson–Lei style algorithms) that
   transfers?

For each relevant paper: full reference, one paragraph on what it does and
how it relates to the idea above (same operator, same starting set, only
one of the two, or a different framing), and whether a PDF is openly
available. Please distinguish what you verified in the paper from what
you inferred from titles or abstracts.
