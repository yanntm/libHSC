# `petri/louvain/` — community detection, vendored

Louvain modularity clustering, adopted from **Louvain-BinaryBuild**
(<https://github.com/lip6/Louvain-BinaryBuild>), the multi-criteria community
detection of Blondel–Guillaume–Lambiotte–Lefebvre (2008) and
Campigotto–Conde Céspedes–Guillaume (2014). GPL/LGPL-3.0-or-later — compatible
with libHSC, attribution here.

## Interface — one service, edges in, tree out

The module renders exactly one service: given a weighted undirected graph, return
its community hierarchy. Upstream exposed this as command-line binaries trading
files (a graph in, a `.tree` out); `community.h` is that same interface with the
file replaced by a return value. The level loop, renumbering and aggregation stay
*inside* the module (adopted from `main_louvain`); callers speak only edges-in /
tree-out and never touch `Graph`/`Louvain`/`Quality`. In-memory is a border
convenience, not licence to specialise the module.

## Vendored files, and only these

`graph_binary`, `quality`, `modularity`, `louvain` (`.h`+`.cpp`) — the modularity
path. The other quality criteria (zahn, owzad, goldberg, …) and the CLI mains are
**not** vendored. `community.{h,cpp}` is ours: the in-memory driver.

## Curation (what we changed)

* Wrapped each file in `namespace hsc::petri::louvain`, and replaced global
  `using namespace std;` in the **headers** with scoped `using`-declarations —
  no `std` leaks to includers. The `.cpp` keep a namespace-scoped `using
  namespace std;`.
* Deleted the global `to_string` template from `quality.h` (it clashed with
  `std::to_string`; nothing used it).
* Rewrote the internal `#include "x.h"` to `#include "hsc/petri/louvain/x.h"`.

Otherwise the algorithm is untouched. Built (with `community.cpp` and the Petri
side) into the `petri_import` library, deliberately without our strict warnings.
