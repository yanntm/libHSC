# `tests/cegar/` — the CEGAR package suite

Own doctest binary (`hsc_cegar_tests`), one file per spec milestone
group; the oracles of spec §7 are named in the test cases.

* `test_model.cc` — M0: `.cts` round-trip, the paper's §6 worked
  example (holds by `mono`; the bugged variant violates and refires).
* `test_classifier.cc` — M1: canonicalize idempotence, language
  equality ⇒ byte equality, chaos.
* `test_teacher_learner.cc` — M2+M3: certification against chaos and
  a too-strict table; L\* driven to exactness on generated leaves
  (index ≤ |Q|+1, language equality both directions).
* `test_loop.cc` — M4+M5: verdict parity vs `mono` over the families
  and a random grid (O1); budget (O3) and interning stats; certificate
  obligations re-checked in-process against a mutation battery (the
  out-of-process `hsc-certcheck` run is the sweep's job, spec §11).
