# One binary for the whole suite: doctest registers test cases across
# translation units, and ctest gets the individual cases through
# doctest_discover_tests.

include(doctest)

add_executable(hsc_tests
  main.cc
  test_hash.cc
  test_intern.cc
  test_cache.cc
  test_stats.cc
  test_timing.cc
)
target_link_libraries(hsc_tests PRIVATE hsc::hsc doctest::doctest)
target_compile_options(hsc_tests PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

doctest_discover_tests(hsc_tests)
