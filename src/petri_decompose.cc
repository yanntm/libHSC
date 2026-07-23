/// \file petri_decompose.cc
/// \brief Synthesise a unit tree for a bare net (Petri side of the Louvain use).
///
/// Two conversions bracket the Louvain module, which stays edges-in / tree-out:
/// the net becomes a place co-occurrence graph (two places linked when a
/// transition touches both, à la the gal GraphBuilder), and the returned level
/// partitions become a nested `unit_tree`. The Louvain module knows nothing of
/// nets or unit trees.

#include "hsc/petri/decompose.hh"

#include <string>
#include <vector>

#include "hsc/petri/MatrixCol.h"
#include "hsc/petri/louvain/community.h"

namespace hsc::petri {

namespace {

/// The all-to-all fallback: for each transition, a clique over its whole flow
/// support, weight 1/(#induced pairs). Used only when the control→write pass
/// yields nothing (imitates the gal GraphBuilder's fallback).
std::vector<louvain::edge> all_to_all(const SparsePetriNet<int>& net) {
  std::vector<louvain::edge> edges;
  const MatrixCol<int>& pre = net.getFlowPT();
  const MatrixCol<int>& post = net.getFlowTP();
  for (std::size_t t = 0; t < net.getTransitionCount(); ++t) {
    SparseArray<int> sup =
        SparseArray<int>::sumProd(1, pre.getColumn(t), 1, post.getColumn(t));
    const std::size_t n = sup.size();
    const double pairs = static_cast<double>(n) * (n - 1) / 2.0;
    if (pairs == 0.0) continue;
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = i + 1; j < n; ++j)
        edges.push_back({static_cast<int>(sup.keyAt(i)),
                         static_cast<int>(sup.keyAt(j)), 1.0 / pairs});
  }
  return edges;
}

/// The dependency graph, imitating the gal GraphBuilder's flow-like strategy:
/// a directed edge from every control (input) place to every place the
/// transition purely writes (net-changed and not itself an input), weighted
/// 1/(|ctrl|·|write|). Read/control places thus link to data, but two co-read
/// places (e.g. a test arc) get no edge between them — read edges are weaker.
/// Tiny self-loops keep isolated places present. Falls back to all-to-all.
std::vector<louvain::edge> cooccurrence(const SparsePetriNet<int>& net) {
  const int places = static_cast<int>(net.getPlaceCount());
  const MatrixCol<int>& pre = net.getFlowPT();
  const MatrixCol<int>& post = net.getFlowTP();

  std::vector<louvain::edge> edges;
  for (int p = 0; p < places; ++p) edges.push_back({p, p, 0.001});

  bool any = false;
  for (std::size_t t = 0; t < net.getTransitionCount(); ++t) {
    const SparseArray<int>& in = pre.getColumn(t);  // control: input places
    SparseArray<int> sup =
        SparseArray<int>::sumProd(1, in, 1, post.getColumn(t));

    std::vector<int> ctrl;
    for (std::size_t i = 0; i < in.size(); ++i)
      ctrl.push_back(static_cast<int>(in.keyAt(i)));
    std::vector<int> write;  // net-touched places that are not inputs
    for (std::size_t i = 0; i < sup.size(); ++i) {
      const int p = static_cast<int>(sup.keyAt(i));
      if (in.get(static_cast<std::size_t>(p)) == 0) write.push_back(p);
    }

    const double induced =
        static_cast<double>(ctrl.size()) * static_cast<double>(write.size());
    if (induced == 0.0) continue;
    for (int i : ctrl)
      for (int j : write)
        if (i != j) {
          edges.push_back({i, j, 1.0 / induced});
          any = true;
        }
  }
  return any ? edges : all_to_all(net);
}

/// Children of each community at each level: `kids[L][c]` lists the level-(L-1)
/// node indices (places when L==0) in community `c`.
std::vector<std::vector<std::vector<int>>> group(
    const std::vector<std::vector<int>>& levels) {
  std::vector<std::vector<std::vector<int>>> kids(levels.size());
  for (std::size_t L = 0; L < levels.size(); ++L) {
    int nc = 0;
    for (int c : levels[L]) nc = std::max(nc, c + 1);
    kids[L].assign(static_cast<std::size_t>(nc), {});
    for (std::size_t i = 0; i < levels[L].size(); ++i)
      kids[L][static_cast<std::size_t>(levels[L][i])].push_back(
          static_cast<int>(i));
  }
  return kids;
}

/// Materialise the unit for community \p c at level \p L into \p t; places sit
/// directly in their unit, sub-communities become nested subunits.
std::string materialise(int L, int c,
                        const std::vector<std::vector<std::vector<int>>>& kids,
                        const std::vector<std::string>& pnames, unit_tree& t,
                        int& counter) {
  const std::string id = "u" + std::to_string(counter++);
  unit u;
  u.id = id;
  for (int child : kids[static_cast<std::size_t>(L)][static_cast<std::size_t>(c)]) {
    if (L == 0) {
      u.places.push_back(pnames[static_cast<std::size_t>(child)]);
    } else {
      u.subunits.push_back(materialise(L - 1, child, kids, pnames, t, counter));
    }
  }
  t.units.emplace(id, std::move(u));
  return id;
}

}  // namespace

unit_tree decompose(const SparsePetriNet<int>& net) {
  unit_tree t;
  const int places = static_cast<int>(net.getPlaceCount());
  if (places <= 0) return t;

  const std::vector<std::vector<int>> levels =
      louvain::louvain_tree(places, cooccurrence(net));
  if (levels.empty()) return t;

  const std::vector<std::vector<std::vector<int>>> kids = group(levels);
  const std::vector<std::string>& pnames = net.getPnames();
  const int top = static_cast<int>(levels.size()) - 1;

  int counter = 0;
  unit root;
  root.id = "root";
  for (std::size_t c = 0; c < kids[static_cast<std::size_t>(top)].size(); ++c) {
    root.subunits.push_back(
        materialise(top, static_cast<int>(c), kids, pnames, t, counter));
  }
  t.root = "root";
  t.units.emplace("root", std::move(root));
  return t;
}

}  // namespace hsc::petri
