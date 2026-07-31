# `tests/cegar/` — the CEGAR package suite

Own doctest binary (`hsc_cegar_tests`); fixtures are `.hsc` literals
run through the real pipeline (parse → expand → spec → bridge), so the
suite links the surface library. The oracles of spec §7 are named in
the test cases.

* `fixtures.hh` — the worked example and its bugged variant as `.hsc`
  text, plus the build helper (the runner's exact path).
* `rand_model.hh` — the random model population, built directly on the
  core PODs (free-form monitor, no property leaves); bit-exact with
  the historical generator so the pinned regression seeds keep their
  meaning.
* `test_model.cc` — the bridge (spec §5): letter induction, interning
  keys, the derived monitor, fragment refusals; `mono` on the worked
  example, both variants.
* `test_classifier.cc` — canonicalize idempotence, language equality
  ⇒ byte equality, chaos.
* `test_teacher_learner.cc` — certification against chaos and a
  too-strict table; L\* driven to exactness on the random population
  (index ≤ |Q|+1, language equality both directions).
* `test_loop.cc` — verdict parity vs `mono` over the random grid and
  all policies (O1); budget (O3); interning stats (O4); the worked
  example's phenomenon; the certificate emitter's shape; the pinned
  unsoundness-regression seeds.
* `test_certcheck.cc` — the trusted core (O5): emitted certificates
  pass; mutated ones (dropped inv, weakened select, mis-aliased use)
  fail — in-process, through the same entry the runner's
  `(certcheck FILE)` uses.
* `gen_rand.py` — prints a random separable-fragment `.hsc` model with
  its driver commands; the sweep corpus generator (spec §9).
