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
  against nothing. Diagrams are canonical, so "these two computations agree"
  is an integer comparison and any disagreement is a real semantic
  difference. Saturation reorganises the entire evaluation; this is the only
  thing that says it did not change the answer.

* `test_hash.cc` — the combiner is non-commutative and order sensitive
  (the guard against the legacy XOR collision), member-`hash()` dispatch,
  and measured spread of the mixers on dense integers and structured pairs.
