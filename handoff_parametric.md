# handoff — parametric surface

The parametric surface is implemented and validated (spec retired to git
history; results: `research_notes/hsc_parametric_report.md`).

**Symmetric encodings are implemented and validated** (no spec — direct):
an event family whose binder is uniform (index only as an array access at
a constant offset mod N, full range, one binder) survives expansion as a
`family` form; the translator builds its term by recursion over the sort
tree instead of enumerating instances. `HSC_FAMILY=check` (default)
builds both routes and requires the identical code — green on every
sample and every N tried. `HSC_FAMILY=declared` skips the enumerated
build. Measured (`tests/family_scaling.sh`, balanced philosophers):
op terms 21·k − 8 at N = 2^k (vs ~6N enumerated), R nodes = 4k, reach
~0 s at N = 2^20; wall is dominated by the still-enumerated cell
declarations (Θ(N), ~4 s at 2^20). `-DN=` overrides a param from the
hsc command line.

## Engineering queue (next action first)

1. **Report section**: write the symmetric-encoding results into the
   report (gate, scaling table, the mutex finding: `tSC` does not
   certify — dependent partial ranges; `t1`/`t2` do).
2. **Profile the superlinear translator/engine cost** (×4 N → ×12 time
   at N=16384, net of the expander) — still standing from before; the
   declared route sidesteps it for certified families, it remains for
   everything else.

## Theory

1. Counting stays a double (user ruling: it is an order of magnitude,
   not a number to decide with). No exact/log-form cardinality work.
2. Scalarset discipline / orbit tags (quotient reduction) remain
   deferred as spec §3 says.

## Blockers

None.
