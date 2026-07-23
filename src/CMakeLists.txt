# Non-header implementation of the library. Sources are listed explicitly:
# globbing hides additions from CMake's dependency tracking.

target_sources(hsc PRIVATE
  stats.cc
  timing.cc
  int_set.cc
  diagram.cc
  operation.cc
  surface_parser.cc
  surface_translate.cc
  query.cc
)
