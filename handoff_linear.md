# Handoff — linear constraints and diagrams (`include/hsc/linear/`)

Current state only, next action first. Design: `include/hsc/linear/algorithm.md`.
Ledger: `research_notes/ideas.md` #1, #2, #3, #17, #18.

## Engineering — next

1. **The one-step test on BugTracking's 2769 survivors** (`--dead-step`):
   how many of ITS-Tools' 1098 SMT kills it reaches, and the cost of the
   image of the invariant set (8905 nodes under Sloan). Then iterate: flows
   of the net without its dead transitions (fewer transitions, more flows,
   tighter bounds), test again.
2. **Mixed-sign flows as constraints**: the knapsack takes nonnegative
   coefficients only; a flow `Σ c·m = K` with signs needs residuals in both
   directions (bounded by the box) — or a split into two inequalities.
3. **ITS-Tools as a caller**: its dead-transition search is the same
   propagation then SMT (61 s on BugTracking); `hsc-pn --dead` as a
   replacement or a complement, through the `-hsc` plug (reduced nets in,
   dead transitions out), measured on the corpus against the QLA oracles.
4. **ω-values** in the leaf theory for the uncovered places (ledger #2):
   today their guard atoms are dropped; a ceiling would keep them.
5. **Learning invariants from `S ∖ R`** (ledger #17) and hard reachability
   questions from the gap (#18) once `S` and `R` both exist for a corpus.

## Known

`(equality …)` must take its domains from the box (a bug found on
SupplyChain: the declared domains under-approximated, one false "dead").
The shape decides the set's cost as it does the reachable set's.
