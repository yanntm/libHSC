# The CEGAR package suite: its own binary. Fixtures are `.hsc` literals
# through the real pipeline, so the suite links the surface library.
add_executable(hsc_cegar_tests
  ../main.cc
  test_model.cc
  test_classifier.cc
  test_teacher_learner.cc
  test_loop.cc
  test_certcheck.cc
)
target_link_libraries(hsc_cegar_tests PRIVATE hsc hsc_cegar doctest::doctest)
target_compile_options(hsc_cegar_tests PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

doctest_discover_tests(hsc_cegar_tests)
