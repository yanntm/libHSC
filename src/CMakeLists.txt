# Non-header implementation of the library. Sources are listed explicitly:
# globbing hides additions from CMake's dependency tracking.

# The vendored Louvain method (adopted, not authored: no strict warnings),
# shared by the Petri importer and the (decompose-louvain) rewrite pass.
add_library(hsc_louvain STATIC
  louvain/graph_binary.cpp
  louvain/quality.cpp
  louvain/modularity.cpp
  louvain/louvain.cpp
  louvain/community.cpp)
target_include_directories(hsc_louvain PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hsc PUBLIC hsc_louvain)

# The CEGAR package: separate library, not folded into hsc (isolated).
add_subdirectory(cegar)
target_link_libraries(hsc PUBLIC hsc_cegar)

target_sources(hsc PRIVATE
  stats.cc
  timing.cc
  int_set.cc
  diagram.cc
  operation.cc
  surface_parser.cc
  surface_expand.cc
  surface_expr.cc
  surface_translate.cc
  surface_events.cc
  surface_families.cc
  surface_query.cc
  surface_weighted_count.cc
  surface_run.cc
  surface_rewrite.cc
  surface_hotbit.cc
  surface_reorder.cc
  surface_arrays.cc
  surface_louvain.cc
  surface_xpl.cc
  surface_cegar.cc
  surface_certcheck.cc
  surface_spec.cc
  dve_parser.cc
  dve_to_surface.cc
  lia_bool.cc
  lia_int.cc
  query.cc
  force.cc
  event.cc
  xpl/eval.cc
  xpl/model.cc
  xpl/fire.cc
  xpl/engine.cc
  xpl/domains.cc
)
