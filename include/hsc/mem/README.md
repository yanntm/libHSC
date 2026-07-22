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

* `id_table.hh` — the unique table's content-addressed direction: a flat
  open-addressed set of ids, no tombstones, hashing and comparison supplied
  per call so it is not templated on the value type.

* `intern.hh` — **the** unique table, `intern<T, Id = uint32_t>`. One
  mechanism for every interned kind: nodes, shapes, terms, residuals,
  kernels. Lookup is by *view* — a description of a value that does not
  exist yet — so a hit allocates and constructs nothing. Trailing arrays are
  supported from the start, because that is the shape a diagram node has.
  Collection is mark & sweep from the referenced ids, iterative, rebuilding
  the probe table rather than erasing from it. Freeing an id bumps its
  generation, which is what makes a citation across a collection checkable.

* `handle.hh` — `weak<T>` (a bare id: what edges hold, no refcount
  traffic), `strong<T>` (a refcounted root), `certificate<T>` (id plus
  generation: the only honest citation across a collection).

## Pending (M1)

* `cache.hh` — the bounded operation cache.
* `manager.hh` — the owner of tables and caches; no singletons.

See `algorithm.md` for the designs, which are written before the code.
