# Handoff — linear constraints and diagrams (`include/hsc/linear/`)

Current state only, next action first. Design: `include/hsc/linear/algorithm.md`.
Ledger: `research_notes/ideas.md` #1, #2, #3, #17, #18.

## Engineering — next

1. **The one-step test backward, per slice** (`algorithm.md` §2, the
   backward reading): the forward image of `S` does not return on
   BugTracking (385 s, under all events or the 2769 live ones). Compute
   `pre(en(t)(S)) ∩ (S ∖ en(t)(S))` instead, with the converses restricted
   to `S` and to the live producers of the input places of `t`; iterate
   while a transition falls. Before it, the **abstraction of uncovered
   places** at the net level in hsc-pn (places and arcs removed, not
   capped), which makes the step sound without the exactness gate. Target:
   the 2769 survivors, against the 1098 ITS-Tools kills by SMT (61 s).
   Then `(reach B backward from X within S)` for a general target `X`:
   one step refutes, the fixpoint decides.
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

6. **`S` as the first pass of every question** (ledger #19): `hsc-pn --approx`
   builds `S` before `R` and answers the reachability and invariant
   properties it refutes (a selector each); then the CTL checker's `EF`
   leaves; keep the best `S` of a run as a named result the driver reads.
7. **Behind the ITS-Tools reducer** (`BK_TOOL=hscxred` in the harness):
   on StateSpace the `-rebuildPNML` branch returns before any reduction
   (Application.java, the `if (rebuildPNML)` right after `createSPN`), so
   the exported net is the raw one — BugTracking came out with its 27 370
   transitions, and hsc-pn answered nothing in 300 s. Moving that block
   below the StateSpace reductions (constant places, redundant transitions,
   `applyReductions(STATESPACE)`) is a one-line change in ITS-Tools; the
   reduced nets are the population where `S` and `R` both change.

## Known

`(equality …)` must take its domains from the box (a bug found on
SupplyChain: the declared domains under-approximated, one false "dead").
The shape decides the set's cost as it does the reachable set's.
