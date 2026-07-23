// community.cpp — in-memory driver for the Louvain module.
//
// Adopts the level loop of upstream `main_louvain` (build a graph, then repeat
// one_level + aggregate while quality improves), returning the per-level
// partitions instead of writing a .tree file. Everything here is module-
// internal: reading `qual->n2c` and calling `partition2graph_binary` is the same
// thing `main_louvain` did, kept behind the edges-in / tree-out boundary.

#include "hsc/petri/louvain/community.h"

#include <map>
#include <vector>

#include "hsc/petri/louvain/graph_binary.h"
#include "hsc/petri/louvain/louvain.h"
#include "hsc/petri/louvain/modularity.h"

namespace hsc::petri::louvain {
using namespace std;

namespace {

/// Build the CSR Graph from an edge list, undirected, weights summed. Mirrors
/// the post-conditions of the binary-file constructor (total_weight, nodes_w).
Graph build(int n, const vector<edge>& edges) {
  vector<map<int, long double>> adj(static_cast<size_t>(n));
  for (const edge& e : edges) {
    adj[e.src][e.dest] += static_cast<long double>(e.weight);
    if (e.src != e.dest) adj[e.dest][e.src] += static_cast<long double>(e.weight);
  }
  Graph g;
  g.nb_nodes = n;
  g.degrees.resize(static_cast<size_t>(n));
  unsigned long long cum = 0;
  for (int i = 0; i < n; i++) {
    cum += adj[static_cast<size_t>(i)].size();
    g.degrees[static_cast<size_t>(i)] = cum;
  }
  g.nb_links = cum;
  g.links.reserve(cum);
  g.weights.reserve(cum);
  for (int i = 0; i < n; i++)
    for (const auto& [j, w] : adj[static_cast<size_t>(i)]) {
      g.links.push_back(j);
      g.weights.push_back(w);
    }
  g.total_weight = 0.0L;
  for (int i = 0; i < n; i++) g.total_weight += g.weighted_degree(i);
  g.nodes_w.assign(static_cast<size_t>(n), 1);
  g.sum_nodes_w = n;
  return g;
}

/// The current partition, communities renumbered 0..k-1 by first appearance —
/// exactly `display_partition`/`partition2graph_binary`, captured not printed.
vector<int> renumbered(const Louvain& c) {
  const int size = c.qual->size;
  vector<int> renum(static_cast<size_t>(size), -1);
  for (int node = 0; node < size; node++) renum[c.qual->n2c[node]]++;
  int end = 0;
  for (int i = 0; i < size; i++)
    if (renum[static_cast<size_t>(i)] != -1) renum[static_cast<size_t>(i)] = end++;
  vector<int> part(static_cast<size_t>(size));
  for (int i = 0; i < size; i++)
    part[static_cast<size_t>(i)] = renum[c.qual->n2c[i]];
  return part;
}

}  // namespace

vector<vector<int>> louvain_tree(int nb_nodes, const vector<edge>& edges,
                                 long double precision) {
  vector<vector<int>> levels;
  if (nb_nodes <= 0) return levels;

  Graph g = build(nb_nodes, edges);
  bool improvement = true;
  do {
    Modularity q(g);
    Louvain c(-1, precision, &q);
    improvement = c.one_level();
    levels.push_back(renumbered(c));
    g = c.partition2graph_binary();  // aggregate for the next level
  } while (improvement && g.nb_nodes > 1);
  return levels;
}

}  // namespace hsc::petri::louvain
