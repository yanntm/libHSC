# The CEGAR package: its own static library, isolated from the calculus —
# depends on util/ headers only (see include/hsc/cegar/README.md).
add_library(hsc_cegar STATIC
  model.cc
  classifier.cc
  certify.cc
  learner.cc
  loop.cc
  gen.cc)
target_include_directories(hsc_cegar PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_options(hsc_cegar PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
