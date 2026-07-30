# experiments/cegar — v1 campaign records

Spec: `research_notes/cegar_spec.md` §10; report:
`research_notes/cegar_report.md`. Every TSV here is produced by
`./sweep.sh BUILDDIR` (typically `./sweep.sh ../../build`); models are
generated on the fly by `hsc-cegar gen` with the seeds recorded in the
rows, so the TSVs are reproducible from the script alone.

* `ta_tb_parity.tsv` — T-A verdict parity + T-B budget/cone, one row
  per model: families (clients, clients-bug, ring at 4 sizes) and two
  `rand` grids of 100 seeds each. `agree` must be `yes` everywhere;
  `certcheck` must be `pass` on every `holds` row (the independent
  checker runs inside the sweep). `cex ≤ budget` is oracle O3 as data.
* `tc_scaling.tsv` — T-C symmetry scaling: clients/ring at
  n ∈ {2..64}, interning on vs off; run vs mono states and walltime.
* `td_policies.tsv` — T-D policy knobs on the rand corpus:
  {all, first, cheapest} × jump-exact {off, on}.

Timeouts appear as `timeout` rows (15 s per model run), never as
discarded rows.
