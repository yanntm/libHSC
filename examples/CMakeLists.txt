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

# The file surface: a runner that parses, translates and executes a .hsc model.
add_executable(hscrun hsc.cc)
target_link_libraries(hscrun PRIVATE hsc::hsc)
target_compile_options(hscrun PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

if(HSC_BUILD_TESTS)
  add_test(NAME surface_hanoi3
           COMMAND hscrun ${CMAKE_CURRENT_SOURCE_DIR}/models/hanoi3.hsc)
  add_test(NAME surface_ring4
           COMMAND hscrun ${CMAKE_CURRENT_SOURCE_DIR}/models/ring4.hsc)
  add_test(NAME surface_ring4b
           COMMAND hscrun ${CMAKE_CURRENT_SOURCE_DIR}/models/ring4b.hsc)
  add_test(NAME surface_unbounded
           COMMAND hscrun ${CMAKE_CURRENT_SOURCE_DIR}/models/unbounded.hsc)
  add_test(NAME surface_states
           COMMAND hscrun ${CMAKE_CURRENT_SOURCE_DIR}/models/states.hsc)
  add_test(NAME surface_altseq
           COMMAND hscrun ${CMAKE_CURRENT_SOURCE_DIR}/models/altseq.hsc)
endif()
