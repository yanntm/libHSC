# The CEGAR package suite: its own binary, linking only the package.
add_executable(hsc_cegar_tests
  ../main.cc
  test_model.cc
  test_classifier.cc
  test_teacher_learner.cc
  test_loop.cc
)
target_link_libraries(hsc_cegar_tests PRIVATE hsc_cegar doctest::doctest)
target_compile_options(hsc_cegar_tests PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

doctest_discover_tests(hsc_cegar_tests)
