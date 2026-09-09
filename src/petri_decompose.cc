/// \file petri_decompose.cc
/// \brief Synthesise a unit tree for a bare net (Petri side of the Louvain use).
///
/// Two conversions bracket the Louvain module, which stays edges-in / tree-out:
/// the net becomes a place co-occurrence graph (two places linked when a
/// transition touches both, à la the gal GraphBuilder), and the returned level
/// partitions become a nested `unit_tree`. The Louvain module knows nothing of
/// nets or unit trees.

#include "hsc/petri/decompose.hh"

#include <algorithm>
#include <string>
#include <vector>

#include "hsc/petri/core/MatrixCol.h"
#include "hsc/petri/louvain/community.h"
#include "hsc/petri/louvain/hyperedge.hh"

#include <cstdlib>

namespace hsc::petri {

namespace {

/// The all-to-all fallback: for each transition, a clique over its whole flow
/// support, weight 1/(#induced pairs). Used only when the control→write pass
/// yields nothing (imitates the gal GraphBuilder's fallback). A transition
/// whose clique exceeds `louvain::max_induced` is skipped (`hyperedge.hh`).
std::vector<louvain::edge> all_to_all(const SparsePetriNet<int>& net) {
  std::vector<louvain::edge> edges;
  const MatrixCol<int>& pre = net.getFlowPT();
  const MatrixCol<int>& post = net.getFlowTP();
  for (std::size_t t = 0; t < net.getTransitionCount(); ++t) {
    SparseArray<int> sup =
        SparseArray<int>::sumProd(1, pre.getColumn(t), 1, post.getColumn(t));
    const std::size_t n = sup.size();
    const std::size_t pairs_n = n * (n - 1) / 2;
    if (!louvain::induced_fits(pairs_n)) continue;
    const double pairs = static_cast<double>(pairs_n);
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
/// A transition wider than the `hyperedge.hh` bounds is skipped: it induces a
/// clique quadratic in its support and speaks of synchronisation, not locality.
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

    const std::size_t induced_n = ctrl.size() * write.size();
    if (!louvain::induced_fits(induced_n) || !louvain::control_fits(ctrl.size()))
      continue;
    const double induced = static_cast<double>(induced_n);
    for (int i : ctrl)
      for (int j : write)
        if (i != j) {
          edges.push_back({i, j, 1.0 / induced});
          any = true;
        }
  }
  return any ? edges : all_to_all(net);
}

/// The flows as hyperedges. A flow's support is a clique, weight
/// `HSC_INV_WEIGHT` (default 1, a variation point) shared over its pairs; a
/// support wider than the hyperedge bounds is left out, like a wide
/// transition: a global invariant says nothing about locality. Signs matter:
/// two places with the same coefficient sign are substitutes (their weighted
/// sum is what the flow conserves, a semiflow hidden in it), two with opposite
/// signs co-move; the cross-sign pairs get the fraction `HSC_INV_CROSS`
/// (default 0.5) of the weight.
void add_invariant_edges(std::vector<louvain::edge>& edges,
                         std::span<const pflow> invariants,
                         const std::vector<int>& node_of) {
  static const double weight = [] {
    const char* e = std::getenv("HSC_INV_WEIGHT");
    return e == nullptr ? 1.0 : std::atof(e);
  }();
  static const double cross = [] {
    const char* e = std::getenv("HSC_INV_CROSS");
    return e == nullptr ? 0.5 : std::atof(e);
  }();
  for (const pflow& f : invariants) {
    const std::size_t n = f.terms.size();
    const std::size_t pairs_n = n * (n - 1) / 2;
    if (n < 2 || !louvain::induced_fits(pairs_n)) continue;
    const double w = weight / static_cast<double>(pairs_n);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = i + 1; j < n; ++j) {
        const int a = node_of[static_cast<std::size_t>(f.terms[i].first)];
        const int b = node_of[static_cast<std::size_t>(f.terms[j].first)];
        if (a == b) continue;  // contracted together already
        const bool same = (f.terms[i].second > 0) == (f.terms[j].second > 0);
        edges.push_back({a, b, same ? w : cross * w});
      }
  }
}

/// \brief The contraction: places tied by a flow whose constant is at least
/// `HSC_INV_MERGE` (default 0: none) become one node for the clustering, and
/// one flat unit afterwards. A conservation of many tokens across a cluster
/// frontier is what the hierarchy cannot afford — the frontier head would
/// carry the whole distribution — so those places are kept side by side.
struct contraction {
  std::vector<int> node_of;               ///< place -> node
  std::vector<std::vector<int>> members;  ///< node -> its places, ascending
};

contraction contract(int places, std::span<const pflow> invariants) {
  static const long long merge = [] {
    const char* e = std::getenv("HSC_INV_MERGE");
    return e == nullptr ? 0LL : std::atoll(e);
  }();
  std::vector<int> parent(static_cast<std::size_t>(places));
  for (int p = 0; p < places; ++p) parent[static_cast<std::size_t>(p)] = p;
  const auto find = [&](int p) {
    while (parent[static_cast<std::size_t>(p)] != p) {
      parent[static_cast<std::size_t>(p)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(p)])];
      p = parent[static_cast<std::size_t>(p)];
    }
    return p;
  };
  if (merge > 0) {
    for (const pflow& f : invariants) {
      if (f.constant < merge || f.terms.empty()) continue;
      const int r = find(f.terms.front().first);
      for (const auto& [q, k] : f.terms) parent[static_cast<std::size_t>(find(q))] = r;
    }
  }
  contraction c;
  c.node_of.assign(static_cast<std::size_t>(places), -1);
  std::vector<int> node_of_root(static_cast<std::size_t>(places), -1);
  for (int p = 0; p < places; ++p) {
    const int r = find(p);
    int& n = node_of_root[static_cast<std::size_t>(r)];
    if (n < 0) {
      n = static_cast<int>(c.members.size());
      c.members.emplace_back();
    }
    c.node_of[static_cast<std::size_t>(p)] = n;
    c.members[static_cast<std::size_t>(n)].push_back(p);
  }
  return c;
}

/// Edges over places, rewritten over the contraction's nodes.
std::vector<louvain::edge> contracted(std::vector<louvain::edge> edges,
                                      const std::vector<int>& node_of) {
  for (louvain::edge& e : edges) {
    e.src = node_of[static_cast<std::size_t>(e.src)];
    e.dest = node_of[static_cast<std::size_t>(e.dest)];
  }
  return edges;
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
                        const contraction& q,
                        const std::vector<std::string>& pnames, unit_tree& t,
                        int& counter) {
  const std::string id = "u" + std::to_string(counter++);
  unit u;
  u.id = id;
  for (int child : kids[static_cast<std::size_t>(L)][static_cast<std::size_t>(c)]) {
    if (L == 0) {
      // a level-0 node is a contraction node: its places, side by side
      for (int p : q.members[static_cast<std::size_t>(child)])
        u.places.push_back(pnames[static_cast<std::size_t>(p)]);
    } else {
      u.subunits.push_back(materialise(L - 1, child, kids, q, pnames, t, counter));
    }
  }
  t.units.emplace(id, std::move(u));
  return id;
}

}  // namespace

unit_tree decompose(const SparsePetriNet<int>& net, std::span<const pflow> invariants) {
  unit_tree t;
  const int places = static_cast<int>(net.getPlaceCount());
  if (places <= 0) return t;

  const contraction q = contract(places, invariants);
  std::vector<louvain::edge> edges = contracted(cooccurrence(net), q.node_of);
  add_invariant_edges(edges, invariants, q.node_of);
  const std::vector<std::vector<int>> levels =
      louvain::louvain_tree(static_cast<int>(q.members.size()), std::move(edges));
  if (levels.empty()) return t;

  const std::vector<std::vector<std::vector<int>>> kids = group(levels);
  const std::vector<std::string>& pnames = net.getPnames();
  const int top = static_cast<int>(levels.size()) - 1;

  int counter = 0;
  unit root;
  root.id = "root";
  for (std::size_t c = 0; c < kids[static_cast<std::size_t>(top)].size(); ++c) {
    root.subunits.push_back(
        materialise(top, static_cast<int>(c), kids, q, pnames, t, counter));
  }
  t.root = "root";
  t.units.emplace("root", std::move(root));
  return t;
}

}  // namespace hsc::petri
