/// \file petri_decompose.cc
/// \brief Synthesise a unit tree for a bare net (Petri side of the Louvain use).
///
/// Two conversions bracket the ordering package (`order/cluster.hh`), which
/// stays edges-in / groups-out: the net becomes a place co-occurrence graph
/// (two places linked when a transition touches both, à la the gal
/// GraphBuilder) and its flows become signed supports; the nested groups
/// returned become a `unit_tree`. The package knows nothing of nets.

#include "hsc/petri/decompose.hh"

#include <algorithm>
#include <string>
#include <vector>

#include "hsc/petri/core/MatrixCol.h"
#include "hsc/order/cluster.hh"
#include "hsc/order/louvain/community.h"
#include "hsc/order/louvain/hyperedge.hh"

namespace hsc::petri {

namespace {

/// The all-to-all fallback: for each transition, a clique over its whole flow
/// support, weight 1/(#induced pairs). Used only when the control→write pass
/// yields nothing (imitates the gal GraphBuilder's fallback). A transition
/// whose clique exceeds `order::louvain::max_induced` is skipped (`hyperedge.hh`).
std::vector<order::louvain::edge> all_to_all(const SparsePetriNet<int>& net) {
  std::vector<order::louvain::edge> edges;
  const MatrixCol<int>& pre = net.getFlowPT();
  const MatrixCol<int>& post = net.getFlowTP();
  for (std::size_t t = 0; t < net.getTransitionCount(); ++t) {
    SparseArray<int> sup =
        SparseArray<int>::sumProd(1, pre.getColumn(t), 1, post.getColumn(t));
    const std::size_t n = sup.size();
    const std::size_t pairs_n = n * (n - 1) / 2;
    if (!order::louvain::induced_fits(pairs_n)) continue;
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
std::vector<order::louvain::edge> cooccurrence(const SparsePetriNet<int>& net) {
  const int places = static_cast<int>(net.getPlaceCount());
  const MatrixCol<int>& pre = net.getFlowPT();
  const MatrixCol<int>& post = net.getFlowTP();

  std::vector<order::louvain::edge> edges;
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
    if (!order::louvain::induced_fits(induced_n) || !order::louvain::control_fits(ctrl.size()))
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

/// The flows as the clustering\'s signed supports.
std::vector<order::signed_support> supports(std::span<const pflow> invariants) {
  std::vector<order::signed_support> out;
  out.reserve(invariants.size());
  for (const pflow& f : invariants) out.push_back({f.terms, f.constant});
  return out;
}

/// Materialise the unit for community \p c at level \p L into \p t; places sit
/// directly in their unit, sub-communities become nested subunits.
std::string materialise(int L, int c, const order::hierarchy& h,
                        const std::vector<std::string>& pnames, unit_tree& t,
                        int& counter) {
  const std::string id = "u" + std::to_string(counter++);
  unit u;
  u.id = id;
  for (int child : h.kids[static_cast<std::size_t>(L)][static_cast<std::size_t>(c)]) {
    if (L == 0) {
      // a level-0 node is a contraction node: its places, side by side
      for (int p : h.members[static_cast<std::size_t>(child)])
        u.places.push_back(pnames[static_cast<std::size_t>(p)]);
    } else {
      u.subunits.push_back(materialise(L - 1, child, h, pnames, t, counter));
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

  const order::hierarchy h = order::cluster(places, cooccurrence(net), supports(invariants));
  if (h.kids.empty()) return t;

  const std::vector<std::string>& pnames = net.getPnames();
  int counter = 0;
  unit root;
  root.id = "root";
  for (std::size_t c = 0; c < h.kids[static_cast<std::size_t>(h.top())].size(); ++c) {
    root.subunits.push_back(materialise(h.top(), static_cast<int>(c), h, pnames, t, counter));
  }
  t.root = "root";
  t.units.emplace("root", std::move(root));
  return t;
}

}  // namespace hsc::petri
