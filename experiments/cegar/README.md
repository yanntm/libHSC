# experiments/cegar — campaign records

Spec: `research_notes/cegar_spec.md` §10; report:
`research_notes/cegar_report.md`. Every TSV here is produced by
`./sweep.sh BUILDDIR` (typically `./sweep.sh ../../build`); models are
the `examples/cegar/*_model.hsc` files sized by `-DK`/`-DN`, so the
TSVs are reproducible from the script and the repository alone.

Per row, the **parity triangle** — three independent implementations:
the `cegar` verdict, the symbolic engine (`reach` + `select` of the
bad atoms), and the explicit engine's count (`xreach`); on every
`holds`, the certificate is exported and re-checked by `certcheck`
inside the sweep. `agree` must be `yes` and `certcheck` `pass` on
every row; `cex ≤ budget` is oracle O3 as data. Timeouts appear as
status rows (15 s per run), never as discarded rows.

* `ta_parity.tsv` — T-A: clients, clients-bug, ring at sizes
  {2,3,5,8}; verdicts, agreement, certcheck, budget, rung profile,
  |Inv|, the ns split (search/replay/refine — refine contains the
  leaf oracles), wall times.
* `tc_scaling.tsv` — T-C: clients(K), ring(N) for
  K,N ∈ {2,4,8,16,32,64}, interning on vs off. The headline is the
  ring: interned refinement is flat in N (4 counterexamples at every
  size — per behavior, not per instance) while the no-intern ablation
  pays ~2N; clients' server contract grows with K by design (the
  paper's §8 overhead pole).

There is no random corpus here: random models are a correctness
fuzzer and live in `tests/cegar/` (in-process, against the `mono`
oracle). T-D (policy knobs) is deferred until the refinement-heavy
corpus exists — the families resolve in too few rounds for policies
to diverge.
