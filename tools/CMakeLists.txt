# Importers and generators. The NUPN importer emits `.hsc` text and does not
# link the calculus core; it needs only expat (for PNML), so it is gated on it.

find_package(EXPAT)
if(EXPAT_FOUND)
  add_executable(nupn2hsc
    nupn2hsc.cc
    ${PROJECT_SOURCE_DIR}/src/petri_nupn.cc
    ${PROJECT_SOURCE_DIR}/src/petri_to_surface.cc)
  target_include_directories(nupn2hsc PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(nupn2hsc PRIVATE EXPAT::EXPAT)
  target_compile_options(nupn2hsc PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

  # The MCC runner links both the importer (expat) and the calculus core.
  add_executable(hsc-mcc
    hsc-mcc.cc
    ${PROJECT_SOURCE_DIR}/src/petri_nupn.cc
    ${PROJECT_SOURCE_DIR}/src/petri_to_surface.cc)
  target_include_directories(hsc-mcc PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(hsc-mcc PRIVATE hsc::hsc EXPAT::EXPAT)
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
  message(STATUS "expat not found: the nupn2hsc importer will not be built")
endif()
