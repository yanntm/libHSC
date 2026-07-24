# Runnable examples. Each one is also a test: it checks its own output
# against an independent oracle and exits nonzero if they disagree.

add_executable(philosophers philosophers.cc)
target_link_libraries(philosophers PRIVATE hsc::hsc)
target_compile_options(philosophers PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

if(HSC_BUILD_TESTS)
  add_test(NAME example_philosophers COMMAND philosophers)
endif()

add_executable(hanoi hanoi.cc)
target_link_libraries(hanoi PRIVATE hsc::hsc)
target_compile_options(hanoi PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

if(HSC_BUILD_TESTS)
  add_test(NAME example_hanoi COMMAND hanoi)
endif()

# The .hsc models under models/ are self-checking inputs of the `hsc` CLI
# (built in tools/ as target hsc_cli).
if(HSC_BUILD_TESTS)
  add_test(NAME surface_hanoi3
           COMMAND hsc_cli ${CMAKE_CURRENT_SOURCE_DIR}/models/hanoi3.hsc)
  add_test(NAME surface_ring4
           COMMAND hsc_cli ${CMAKE_CURRENT_SOURCE_DIR}/models/ring4.hsc)
  add_test(NAME surface_ring4b
           COMMAND hsc_cli ${CMAKE_CURRENT_SOURCE_DIR}/models/ring4b.hsc)
  add_test(NAME surface_unbounded
           COMMAND hsc_cli ${CMAKE_CURRENT_SOURCE_DIR}/models/unbounded.hsc)
  add_test(NAME surface_states
           COMMAND hsc_cli ${CMAKE_CURRENT_SOURCE_DIR}/models/states.hsc)
  add_test(NAME surface_altseq
           COMMAND hsc_cli ${CMAKE_CURRENT_SOURCE_DIR}/models/altseq.hsc)
endif()
