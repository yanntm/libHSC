# `examples/cegar/` — certified component abstraction, demonstrated

Self-checking `.hsc` programs for the `cegar` / `certificate` /
`certcheck` commands (manual §8d). Each asserts its verdict with
`expect` and cross-checks the state count against the explicit engine
— verdict parity with an independent walk is the standing oracle.

Every model is a **model-only** file (`*_model.hsc`) pulled in by its
driver via `(input …)` — the split the sweep scripts reuse with size
overrides (`-DK=…`, `-DN=…`).

* `clients_model.hsc` / `clients.hsc` — the cegar paper's worked
  example, K clients and a server, monitor leaf counting outstanding
  grants. **Holds**; the summary line shows the loop's economics
  (counterexamples vs the budget pool, rung profile, abstract states).
* `clients_bug_model.hsc` / `clients_bug.hsc` — grants forget to
  check the server. **Violates**: the witness prints as an event
  word, and the bound result is the validated bad state, re-runnable.
* `ring_model.hsc` — a token ring, model only.
* `ring_check.hsc` — the full round trip on the ring: prove mutual
  exclusion, `certificate` the proof out (as `ring_proof.hsc`, written
  to the working directory, git-ignored here), `certcheck` it back
  through the trusted core, then `xreach` the concrete count.

Run one:

    ./build/tools/hsc examples/cegar/clients.hsc

Package: `include/hsc/cegar/`.
