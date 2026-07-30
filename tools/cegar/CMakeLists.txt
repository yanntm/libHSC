# The CEGAR binaries: the driver (links the package library) and the
# independent certificate checker (self-contained by design).
add_executable(hsc-cegar hsc-cegar.cc)
target_link_libraries(hsc-cegar PRIVATE hsc_cegar)
target_compile_options(hsc-cegar PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(hsc-certcheck hsc-certcheck.cc)
target_compile_options(hsc-certcheck PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
