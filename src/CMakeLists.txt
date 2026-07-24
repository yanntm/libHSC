# Non-header implementation of the library. Sources are listed explicitly:
# globbing hides additions from CMake's dependency tracking.

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
  surface_run.cc
  surface_rewrite.cc
  surface_hotbit.cc
  surface_reorder.cc
  surface_arrays.cc
  surface_xpl.cc
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
