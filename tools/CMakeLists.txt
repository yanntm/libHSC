# Importers and generators. The PNML importer and the Louvain decomposition
# emit `.hsc` text; only hsc-mcc additionally links the calculus core. Everything
# needs expat (PNML), so the tools are gated on it.

find_package(EXPAT)
if(EXPAT_FOUND)
  # The import stack: PNML parsing, NUPN, emission, and the vendored Louvain.
  # Built as one library, deliberately without our strict warnings — the
  # vendored community-detection code is adopted, not authored here.
  add_library(petri_import STATIC
    ${PROJECT_SOURCE_DIR}/src/petri_nupn.cc
    ${PROJECT_SOURCE_DIR}/src/petri_to_surface.cc
    ${PROJECT_SOURCE_DIR}/src/petri_decompose.cc
    ${PROJECT_SOURCE_DIR}/src/louvain/graph_binary.cpp
    ${PROJECT_SOURCE_DIR}/src/louvain/quality.cpp
    ${PROJECT_SOURCE_DIR}/src/louvain/modularity.cpp
    ${PROJECT_SOURCE_DIR}/src/louvain/louvain.cpp
    ${PROJECT_SOURCE_DIR}/src/louvain/community.cpp)
  target_include_directories(petri_import PUBLIC ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(petri_import PUBLIC EXPAT::EXPAT)

  add_executable(nupn2hsc nupn2hsc.cc)
  target_link_libraries(nupn2hsc PRIVATE petri_import)
  target_compile_options(nupn2hsc PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

  # The MCC runner also links the calculus core (the surface translator).
  add_executable(hsc-mcc hsc-mcc.cc)
  target_link_libraries(hsc-mcc PRIVATE petri_import hsc::hsc)
  target_compile_options(hsc-mcc PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

  if(HSC_BUILD_TESTS)
    add_test(NAME mcc_samples
             COMMAND ${CMAKE_COMMAND}
               -DHSC_MCC=$<TARGET_FILE:hsc-mcc>
               -DSAMPLES=${PROJECT_SOURCE_DIR}/samples
               -P ${CMAKE_CURRENT_SOURCE_DIR}/check_samples.cmake)
  endif()
else()
  message(STATUS "expat not found: the nupn2hsc / hsc-mcc tools will not be built")
endif()

# The DVE (BEEM) importer.
add_executable(dve2hsc dve2hsc.cc)
target_link_libraries(dve2hsc PRIVATE hsc::hsc CLI11::CLI11)
target_compile_options(dve2hsc PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)
