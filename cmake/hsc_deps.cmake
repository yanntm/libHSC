# External dependencies, fetched into deps/ (never vendored into git).
#
# Policy (roadmap.md, "Dependencies policy"): the library itself uses google
# sparsehash and nothing else that is not std. Tooling around it may lean on
# more: doctest is the test harness (fetched only with HSC_BUILD_TESTS),
# CLI11 the argument parser of the command-line tools.

include(FetchContent)

# Everything lands under deps/ rather than the build tree, so a rebuild from
# scratch does not re-download and the sources are inspectable in place.
set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/deps" CACHE PATH
    "Where fetched dependencies are unpacked" FORCE)

# --- google sparsehash --------------------------------------------------
#
# Header-only in use, but the headers need one generated file,
# sparsehash/internal/sparseconfig.h, produced by the autotools configure plus
# a single make rule. Nothing is compiled or installed: we run those two steps
# once, at CMake configure time, and then treat src/ as an include directory.

FetchContent_Declare(sparsehash
  GIT_REPOSITORY https://github.com/sparsehash/sparsehash.git
  GIT_TAG        sparsehash-2.0.4
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(sparsehash)

set(HSC_SPARSECONFIG "${sparsehash_SOURCE_DIR}/src/sparsehash/internal/sparseconfig.h")
if(NOT EXISTS "${HSC_SPARSECONFIG}")
  message(STATUS "sparsehash: generating sparseconfig.h")
  execute_process(
    COMMAND "${sparsehash_SOURCE_DIR}/configure" --quiet
    WORKING_DIRECTORY "${sparsehash_SOURCE_DIR}"
    OUTPUT_FILE "${sparsehash_SOURCE_DIR}/hsc_configure.log"
    ERROR_FILE  "${sparsehash_SOURCE_DIR}/hsc_configure.log"
    RESULT_VARIABLE _hsc_rc)
  if(NOT _hsc_rc EQUAL 0)
    message(FATAL_ERROR
      "sparsehash configure failed (rc=${_hsc_rc}); see "
      "${sparsehash_SOURCE_DIR}/hsc_configure.log")
  endif()
  execute_process(
    COMMAND ${CMAKE_MAKE_PROGRAM} src/sparsehash/internal/sparseconfig.h
    WORKING_DIRECTORY "${sparsehash_SOURCE_DIR}"
    OUTPUT_QUIET ERROR_QUIET
    RESULT_VARIABLE _hsc_rc)
  if(NOT _hsc_rc EQUAL 0 OR NOT EXISTS "${HSC_SPARSECONFIG}")
    message(FATAL_ERROR "sparsehash: could not generate sparseconfig.h")
  endif()
endif()

add_library(hsc_sparsehash INTERFACE)
add_library(hsc::sparsehash ALIAS hsc_sparsehash)
target_include_directories(hsc_sparsehash SYSTEM INTERFACE "${sparsehash_SOURCE_DIR}/src")

# --- CLI11 (command-line tools only) ------------------------------------

FetchContent_Declare(cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.5.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cli11)

# --- doctest (tests only) -----------------------------------------------

if(HSC_BUILD_TESTS)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    GIT_SHALLOW    TRUE
  )
  set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(doctest)
  # included, not add_subdirectory'd: this lands in the caller's scope
  list(APPEND CMAKE_MODULE_PATH "${doctest_SOURCE_DIR}/scripts/cmake")
endif()
