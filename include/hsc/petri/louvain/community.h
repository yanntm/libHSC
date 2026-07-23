// community.h — the Louvain module's service, in memory.
//
// libHSC addition (not from upstream): the module renders one service — given a
// weighted undirected graph, return its community hierarchy. Upstream exposed it
// as command-line binaries exchanging files (a graph in, a .tree out); this is
// the same interface with the file replaced by a return value. The level loop,
// renumbering and aggregation stay inside the module (adopted from
// `main_louvain`); callers speak only edges-in / tree-out.
#pragma once

#include <vector>

namespace hsc::petri::louvain {

/// One weighted undirected edge of the input graph.
struct edge {
  int src;
  int dest;
  double weight;
};

/// \brief The community hierarchy of the graph on \p nb_nodes nodes with
/// \p edges, as the per-level partitions (the ".tree"): `result[0][i]` is the
/// community of node `i`; `result[L][c]` the community of level-`(L-1)`
/// community `c`. Communities are renumbered `0..k-1` by first appearance at
/// each level. Empty for an empty graph; a single identity level when the graph
/// has no community structure.
[[nodiscard]] std::vector<std::vector<int>> louvain_tree(
    int nb_nodes, const std::vector<edge>& edges,
    long double precision = 1e-6L);

}  // namespace hsc::petri::louvain
