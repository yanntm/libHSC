# Non-header implementation of the library. Sources are listed explicitly:
# globbing hides additions from CMake's dependency tracking.

# The clustering side of the ordering package: the vendored Louvain method
# (adopted, not authored: no strict warnings) and the hierarchy built on it,
# shared by the Petri importer and the (decompose-louvain) rewrite pass.
add_library(hsc_louvain STATIC
  order/louvain/graph_binary.cpp
  order/louvain/quality.cpp
  order/louvain/modularity.cpp
  order/louvain/louvain.cpp
  order/louvain/community.cpp
  order/cluster.cc)
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
  surface_ctl.cc
  surface_trace.cc
  surface_spec.cc
  dve_parser.cc
  dve_to_surface.cc
  lia_bool.cc
  lia_int.cc
  query.cc
  order/force.cc
  order/profile.cc
  event.cc
  xpl/eval.cc
  xpl/model.cc
  xpl/fire.cc
  xpl/engine.cc
  xpl/domains.cc
  ctl/formula.cc
  ctl/forward.cc
  ctl/checker.cc
  trace/path.cc
  trace/witness.cc
)
