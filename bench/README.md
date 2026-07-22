# `bench/` — measurements

**Empty on purpose.** There is no workload yet.

A benchmark over an access pattern we invented measures the invention. Until
diagrams exist and something real runs through the substrate, any number
produced here would be a self-portrait: it would tell us how our table
behaves on the distribution we chose for it, and we would then tune the table
to that choice. That is optimizing before there is anything to optimize.

What fills this folder, and when:

* **M2** — philosophers statics. The first genuine workload: real node
  shapes, real arities, the real ratio of hits to misses that suffix sharing
  produces, the real survivor ratio at collection. Every M1 claim about
  `mem/` gets its number here, against that.
* **M6** — the parity gate. Legacy libDDD and ITS-tools as external
  baselines on P/T saturation over the standard Petri corpus.

The meters the measurements will read already exist and ship with the code
they measure (`hsc/mem/stats.hh`, `hsc/util/timing.hh`): instrumentation is
part of the engine, benchmarks are not.
