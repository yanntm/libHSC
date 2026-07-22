# `src/` — non-header implementation

Mirrors `include/hsc/`. Anything that need not be visible to the compiler at
every use site belongs here rather than in a header; sources are listed
explicitly in `CMakeLists.txt` (no globbing).

* `stats.cc` — the meter field descriptions and the key:value / CSV
  printers declared in `hsc/mem/stats.hh`.
