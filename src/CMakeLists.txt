# Non-header implementation of the library. Sources are listed explicitly:
# globbing hides additions from CMake's dependency tracking.

target_sources(hsc PRIVATE
  stats.cc
)
