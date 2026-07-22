cmake_minimum_required(VERSION 3.24)
project(libHSC VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # no compiler-specific extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for tooling

option(HSC_BUILD_TESTS "Build the test suite" ON)
option(HSC_BUILD_BENCH "Build the benchmarks" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

include(cmake/hsc_deps.cmake)

# --- the library --------------------------------------------------------

add_library(hsc)
add_library(hsc::hsc ALIAS hsc)
target_include_directories(hsc PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries(hsc PUBLIC hsc::sparsehash)
target_compile_options(hsc PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_subdirectory(src)

if(HSC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(HSC_BUILD_BENCH)
  add_subdirectory(bench)
endif()
