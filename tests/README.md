# `tests/` — correctness

One binary, `hsc_tests`, built from `main.cc` (which supplies doctest's
`main`) plus one `test_<unit>.cc` per unit under test. doctest registers the
cases; `doctest_discover_tests` publishes each one to CTest, so
`ctest --test-dir build` runs them individually and CI reads the result.

Run:

    cmake --build build -j && ctest --test-dir build --output-on-failure

Where a reference implementation exists (the enumerated leaf theory, later),
tests are differential against it rather than against hand-written expected
values.

* `test_operations.cc` — **the differential suite**, and the one that
  matters. Random shapes, random models, and the calculus's own equations as
  the oracle: `saturate == naive fixpoint` over 200 random models,
  `compose` against hand-sequenced application, `sum` against union, `id`
  against nothing, the inverse of every event against a brute-force
  converse over the enumerated reachable states, the fused composition of
  two product terms against their sequence, and the symbolic shortest path
  against an explicit BFS. Diagrams are canonical, so "these two computations agree"
  is an integer comparison and any disagreement is a real semantic
  difference. Saturation reorganises the entire evaluation; this is the only
  thing that says it did not change the answer.

* `test_hash.cc` — the combiner is non-commutative and order sensitive
  (the guard against the legacy XOR collision), member-`hash()` dispatch,
  and measured spread of the mixers on dense integers and structured pairs.

* `test_intern.cc`, `test_cache.cc` — the mem substrate: unique-table
  identity, generations, sweep; cache hit/miss discipline.

* `test_split_equiv.cc` — `split_equiv` on the `int_set` leaf: exact
  partition by a linear expression, overflow loud.

* `test_query.cc` — cross-level criteria against a brute-force oracle:
  every comparator, every position pair, spine and balanced shapes (deep
  heads), and the diagram-level `split_equiv` partition checked exact.

* `test_lia.cc` — the expression package: constants living in the code,
  the factory normal form, ⊥ propagation, overflow-loud folds, currying
  residuals, the `tab[tab[x]]` resolution chain.

* `test_xpl.cc` — the explicit engine through the surface: flat models
  where `xreach` and the symbolic `reach` must agree (ring, a flat binary
  counter, havoc, sequential clauses), the TOP discipline (division by
  zero names its witness), Kleene absorption in guards, and the state
  store's dedup across a rehash.

* `test_ctl.cc` — the CTL formula DAG (interning, negation normal form as
  identities) and the forward conversion on hand formulas against the rules
  of `include/hsc/ctl/algorithm.md`. Verdicts are tested by the
  self-checking `examples/models/ctl_*.hsc`.

* `test_rewrite.cc` — the rewrite chain: constant elision preserves
  counts on both engines, the in-file directive, identity reported.

* `test_stats.cc`, `test_timing.cc` — meters and clocks.

* `dve_sweep.sh` — not a ctest: regenerates `examples/divine/hsc/` from the
  DVE corpus and classifies every model (run / refused-crossing / refused-other /
  timeout) into `examples/divine/status.tsv`; logs under `tests/logs/`.

`counting_chain.py --hsc BIN` checks free-SCC fusion followed by constant
removal, analytic state/token counts, PCONST serialization, empty residual
nets, and repeated reductions. The large-factor CTest case uses an analytic
oracle beyond 64-bit range. Each model invocation has a shared 15-second cap.

`conjunction_model.cc` is an optional real-PNML comparison, not a synthetic
regression suite. Build target `conjunction_model` explicitly and invoke
`timeout -k 1 15 build/tools/conjunction_model MODEL.pnml`; see
`include/hsc/linear/conjunction/README.md` for its scope and assumptions.
