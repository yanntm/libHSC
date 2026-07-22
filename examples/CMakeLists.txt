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
