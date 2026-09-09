/// \file cluster.cc
/// \brief Contraction, signed cliques, Louvain levels (`hsc/order/cluster.hh`).
#include "hsc/order/cluster.hh"

#include <cstdlib>

#include "hsc/order/louvain/hyperedge.hh"

namespace hsc::order {

const invariant_knobs& invariant_knobs::from_env() {
  static const invariant_knobs k = [] {
    invariant_knobs v;
    if (const char* e = std::getenv("HSC_INV_WEIGHT")) v.weight = std::atof(e);
    if (const char* e = std::getenv("HSC_INV_CROSS")) v.cross = std::atof(e);
    if (const char* e = std::getenv("HSC_INV_MERGE")) v.merge = std::atoll(e);
    return v;
  }();
  return k;
}

namespace {

/// Positions to nodes: union-find over the supports to contract, then one
/// node per class in order of first position.
void contract(int positions, std::span<const signed_support> invariants, long long merge,
              std::vector<int>& node_of, std::vector<std::vector<int>>& members) {
  std::vector<int> parent(static_cast<std::size_t>(positions));
  for (int p = 0; p < positions; ++p) parent[static_cast<std::size_t>(p)] = p;
  const auto find = [&](int p) {
    while (parent[static_cast<std::size_t>(p)] != p) {
      auto& pp = parent[static_cast<std::size_t>(p)];
      pp = parent[static_cast<std::size_t>(pp)];
      p = pp;
    }
    return p;
  };
  if (merge > 0) {
    for (const signed_support& f : invariants) {
      if (f.constant < merge || f.members.empty()) continue;
      const int r = find(f.members.front().first);
      for (const auto& [q, k] : f.members) parent[static_cast<std::size_t>(find(q))] = r;
    }
  }
  node_of.assign(static_cast<std::size_t>(positions), -1);
  std::vector<int> node_of_root(static_cast<std::size_t>(positions), -1);
  members.clear();
  for (int p = 0; p < positions; ++p) {
    int& n = node_of_root[static_cast<std::size_t>(find(p))];
    if (n < 0) {
      n = static_cast<int>(members.size());
      members.emplace_back();
    }
    node_of[static_cast<std::size_t>(p)] = n;
    members[static_cast<std::size_t>(n)].push_back(p);
  }
}

/// A support as a clique over nodes: same-sign pairs are substitutes (the
/// conservation hides a semiflow among them), opposite signs co-move and get
/// the cross fraction. A support wider than the hyperedge bounds is left out:
/// a global invariant says nothing about locality.
void add_signed_cliques(std::vector<louvain::edge>& edges,
                        std::span<const signed_support> invariants,
                        const std::vector<int>& node_of, const invariant_knobs& k) {
  for (const signed_support& f : invariants) {
    const std::size_t n = f.members.size();
    const std::size_t pairs_n = n * (n - 1) / 2;
    if (n < 2 || !louvain::induced_fits(pairs_n)) continue;
    const double w = k.weight / static_cast<double>(pairs_n);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = i + 1; j < n; ++j) {
        const int a = node_of[static_cast<std::size_t>(f.members[i].first)];
        const int b = node_of[static_cast<std::size_t>(f.members[j].first)];
        if (a == b) continue;  // contracted together already
        const bool same = (f.members[i].second > 0) == (f.members[j].second > 0);
        edges.push_back({a, b, same ? w : k.cross * w});
      }
  }
}

}  // namespace

hierarchy cluster(int positions, std::vector<louvain::edge> edges,
                  std::span<const signed_support> invariants, const invariant_knobs& knobs) {
  hierarchy h;
  if (positions <= 0) return h;
  std::vector<int> node_of;
  contract(positions, invariants, knobs.merge, node_of, h.members);
  for (louvain::edge& e : edges) {
    e.src = node_of[static_cast<std::size_t>(e.src)];
    e.dest = node_of[static_cast<std::size_t>(e.dest)];
  }
  add_signed_cliques(edges, invariants, node_of, knobs);
  const std::vector<std::vector<int>> levels =
      louvain::louvain_tree(static_cast<int>(h.members.size()), std::move(edges));
  h.kids.resize(levels.size());
  for (std::size_t L = 0; L < levels.size(); ++L) {
    int nc = 0;
    for (int c : levels[L]) nc = std::max(nc, c + 1);
    h.kids[L].assign(static_cast<std::size_t>(nc), {});
    for (std::size_t i = 0; i < levels[L].size(); ++i)
      h.kids[L][static_cast<std::size_t>(levels[L][i])].push_back(static_cast<int>(i));
  }
  return h;
}

}  // namespace hsc::order
