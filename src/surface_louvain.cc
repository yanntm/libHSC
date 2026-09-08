/// \file surface_louvain.cc
/// \brief `(decompose-louvain)`: derive a hierarchical shape from the
/// spec's own dependency structure — Louvain modularity clustering over
/// the control→write co-occurrence graph of the events, mirroring the
/// Petri importer's flow-like strategy (`src/petri_decompose.cc`).
/// Communities become nested `(balanced …)` blocks; members keep their
/// relative frontier order. Reordering/regrouping is a rewriting, and
/// semantically neutral.

#include <sstream>

#include "hsc/petri/louvain/community.h"
#include "hsc/petri/louvain/hyperedge.hh"
#include "hsc/surface/rewrite.hh"
#include "hsc/surface/spec.hh"
#include "hsc/xpl/interpret/model.hh"

namespace hsc::surface {

namespace {

namespace lv = hsc::petri::louvain;

/// Flow-like edges: control (read-only) positions point at written ones,
/// weighted 1/(|ctrl|·|write|); tiny self-loops keep isolated leaves. An event
/// wider than the `hyperedge.hh` bounds is left out: it induces a clique
/// quadratic in its support and speaks of synchronisation, not locality.
std::vector<lv::edge> flow_edges(const xpl::model& m) {
  std::vector<lv::edge> edges;
  for (std::size_t p = 0; p < m.arity; ++p) {
    edges.push_back({static_cast<int>(p), static_cast<int>(p), 0.001});
  }
  bool any = false;
  for (const xpl::event& e : m.events) {
    // control = the guard's reads (the Petri input places); write = the
    // positions the event moves that the guard does not read
    std::vector<int> ctrl;
    for (std::size_t i = 0; i < e.ctrl.size(); ++i) {
      ctrl.push_back(static_cast<int>(e.ctrl.keyAt(i)));
    }
    std::vector<int> write;
    for (std::size_t i = 0; i < e.writes.size(); ++i) {
      const auto p = e.writes.keyAt(i);
      if (!e.ctrl.get(p)) write.push_back(static_cast<int>(p));
    }
    const std::size_t induced_n = ctrl.size() * write.size();
    if (!lv::induced_fits(induced_n) || !lv::control_fits(ctrl.size())) continue;
    const double induced = static_cast<double>(induced_n);
    for (const int i : ctrl) {
      for (const int j : write) {
        if (i != j) {
          edges.push_back({i, j, 1.0 / induced});
          any = true;
        }
      }
    }
  }
  return any ? edges : std::vector<lv::edge>{};
}

/// The block of community \p c at level \p L: leaves at level 0, nested
/// blocks above; a single child collapses to itself.
datum block(int L, int c,
            const std::vector<std::vector<std::vector<int>>>& kids,
            const std::vector<std::string>& names, int line) {
  std::vector<datum> members{datum::atom("balanced", line)};
  for (const int child : kids[static_cast<std::size_t>(L)]
                             [static_cast<std::size_t>(c)]) {
    members.push_back(L == 0
                          ? datum::atom(names[static_cast<std::size_t>(child)],
                                        line)
                          : block(L - 1, child, kids, names, line));
  }
  if (members.size() == 2) return members[1];  // a singleton collapses
  return datum::list(std::move(members), line);
}

}  // namespace

rewrite_result decompose_louvain(std::vector<datum> forms, const datum&) {
  const spec s = spec::read(forms);
  const std::size_t n = s.order().size();
  if (n == 0) return {std::move(forms), false, "no shape"};
  lia::expr_factory ex;
  const expr_reader reader(ex, s);
  const xpl::model m = build_xpl_model(n, ex, reader, s, s.events());
  const std::vector<lv::edge> edges = flow_edges(m);
  if (edges.empty()) {
    return {std::move(forms), false, "no dependency structure to cluster"};
  }
  const std::vector<std::vector<int>> levels =
      lv::louvain_tree(static_cast<int>(n), edges);
  if (levels.empty()) {
    return {std::move(forms), false, "no community structure found"};
  }
  // kids[L][c]: the level-(L-1) nodes (leaves when L == 0) in community c
  std::vector<std::vector<std::vector<int>>> kids(levels.size());
  for (std::size_t L = 0; L < levels.size(); ++L) {
    int ncomm = 0;
    for (const int c : levels[L]) ncomm = std::max(ncomm, c + 1);
    kids[L].assign(static_cast<std::size_t>(ncomm), {});
    for (std::size_t i = 0; i < levels[L].size(); ++i) {
      kids[L][static_cast<std::size_t>(levels[L][i])].push_back(
          static_cast<int>(i));
    }
  }
  const int top = static_cast<int>(levels.size()) - 1;
  const std::size_t roots = kids[static_cast<std::size_t>(top)].size();
  if (roots == 1 &&
      kids[static_cast<std::size_t>(top)][0].size() == (top == 0 ? n : 1)) {
    return {std::move(forms), false, "one community: nothing to group"};
  }
  int line = 1;
  for (datum& f : forms) {
    if (!f.is_list() || f.items().empty() || f.head() != "shape") continue;
    line = f.line();
    std::vector<datum> tops{datum::atom("balanced", line)};
    for (std::size_t c = 0; c < roots; ++c) {
      tops.push_back(block(top, static_cast<int>(c), kids, s.order(), line));
    }
    const datum sort = tops.size() == 2 ? tops[1]
                                        : datum::list(std::move(tops), line);
    f = datum::list({datum::atom("shape", line), sort}, line);
  }
  std::ostringstream trace;
  trace << "Louvain: " << levels.size() << " level"
        << (levels.size() > 1 ? "s" : "") << ", " << roots
        << " top communities over " << n << " leaves";
  return {std::move(forms), true, trace.str()};
}

}  // namespace hsc::surface
