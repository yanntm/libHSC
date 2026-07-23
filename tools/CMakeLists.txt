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
else()
  message(STATUS "expat not found: the nupn2hsc importer will not be built")
endif()
