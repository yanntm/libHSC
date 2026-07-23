# Non-header implementation of the library. Sources are listed explicitly:
# globbing hides additions from CMake's dependency tracking.

target_sources(hsc PRIVATE
  stats.cc
  timing.cc
  int_set.cc
  diagram.cc
  operation.cc
  surface_parser.cc
  surface_expr.cc
  surface_translate.cc
  dve_parser.cc
  dve_to_surface.cc
  lia_bool.cc
  lia_int.cc
  query.cc
  event.cc
)
