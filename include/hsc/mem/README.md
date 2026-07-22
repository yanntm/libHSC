# `mem/` — the interning and caching substrate

Hashing and caching are *the* cross-cutting concern of a decision diagram
library. This folder gets them right once, as comfortable infrastructure,
and nothing above it reimplements them.

Nothing here knows what a diagram is. Every component takes the interned or
cached type as a parameter; the diagram layer is one client among nodes,
operation terms, residuals and kernels.

## Contents

* `stats.hh` — meters. A statistics struct is a plain aggregate that
  describes itself as a list of named `stat_field`s; the printers turn any
  such list into key:value lines (for reading) or a CSV row (for the
  artefacts a report cites). Field descriptions are runtime data, not
  template machinery, so a new meter costs one `fields()` method.
  **Nothing is added to `mem/` without its meter.**

## Pending (M1)

* `intern.hh` — the unique table.
* `handle.hh` — `weak<T>` / `strong<T>`.
* `cache.hh` — the bounded operation cache.
* `manager.hh` — the owner of tables and caches; no singletons.

See `algorithm.md` for the designs, which are written before the code.
