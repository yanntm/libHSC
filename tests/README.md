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

* `test_hash.cc` — the combiner is non-commutative and order sensitive
  (the guard against the legacy XOR collision), member-`hash()` dispatch,
  and measured spread of the mixers on dense integers and structured pairs.
