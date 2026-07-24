/// \file surface_reorder.cc
/// \brief Shape passes. `(reorder-force)` — FORCE at every level of the
/// shape tree, hierarchy undisturbed: at each node the events project
/// onto the children they touch and FORCE orders the children; a final
/// polarity choice keeps the orientation whose event tops sit deepest.
/// `(flatten)` — the spine of the current frontier order, hierarchy
/// deliberately erased. Reordering is a rewriting, and semantically
/// neutral: any shape over the same leaves denotes the same states.

#include <algorithm>
#include <limits>
#include <numeric>
#include <sstream>

#include "hsc/order/force.hh"
#include "hsc/surface/rewrite.hh"
#include "hsc/surface/spec.hh"
#include "hsc/xpl/interpret/model.hh"

namespace hsc::surface {

namespace {

/// The shape as a tree over frontier positions.
struct snode {
  bool is_leaf = false;
  std::string text;  ///< leaf/unit name, or the constructor
  std::uint32_t pos = 0;
  std::vector<snode> kids;
  int line = 0;
};

snode parse_sort(const datum& d, const spec& s) {
  snode n;
  n.line = d.line();
  if (d.is_atom()) {
    n.is_leaf = true;
    n.text = d.text();
    if (const auto p = s.position(d.text())) n.pos = *p;
    else n.pos = std::numeric_limits<std::uint32_t>::max();  // unit
    return n;
  }
  n.text = d.head();
  for (std::size_t i = 1; i < d.items().size(); ++i) {
    n.kids.push_back(parse_sort(d.items()[i], s));
  }
  return n;
}

datum unparse(const snode& n) {
  if (n.is_leaf) return datum::atom(n.text, n.line);
  std::vector<datum> kids{datum::atom(n.text, n.line)};
  for (const snode& k : n.kids) kids.push_back(unparse(k));
  return datum::list(std::move(kids), n.line);
}

void frontier(const snode& n, std::vector<std::uint32_t>& out) {
  if (n.is_leaf) {
    if (n.pos != std::numeric_limits<std::uint32_t>::max()) out.push_back(n.pos);
    return;
  }
  for (const snode& k : n.kids) frontier(k, out);
}

/// FORCE the children of every node: each event's clique is the set of
/// children it touches. Returns the number of nodes whose order moved.
std::size_t reorder(snode& n, const xpl::model& m,
                    std::vector<int>& child_of) {
  if (n.is_leaf || n.kids.size() < 2) return 0;
  std::fill(child_of.begin(), child_of.end(), -1);
  for (std::size_t i = 0; i < n.kids.size(); ++i) {
    std::vector<std::uint32_t> ps;
    frontier(n.kids[i], ps);
    for (const std::uint32_t p : ps) child_of[p] = static_cast<int>(i);
  }
  std::vector<order::clique> cliques;
  for (const xpl::event& e : m.events) {
    std::vector<std::uint32_t> touched;
    const auto touch = [&](const SparseBoolArray& sba) {
      for (std::size_t i = 0; i < sba.size(); ++i) {
        const int c = child_of[sba.keyAt(i)];
        if (c >= 0) touched.push_back(static_cast<std::uint32_t>(c));
      }
    };
    touch(e.reads);
    touch(e.writes);
    std::sort(touched.begin(), touched.end());
    touched.erase(std::unique(touched.begin(), touched.end()),
                  touched.end());
    if (touched.size() >= 2) cliques.push_back({std::move(touched), 1.0f});
  }
  std::size_t moved = 0;
  if (!cliques.empty()) {
    const std::vector<std::uint32_t> perm =
        order::force(n.kids.size(), cliques, {});
    std::vector<std::uint32_t> identity(perm.size());
    std::iota(identity.begin(), identity.end(), 0);
    if (perm != identity) {
      std::vector<snode> nk;
      nk.reserve(n.kids.size());
      for (const std::uint32_t r : perm) nk.push_back(std::move(n.kids[r]));
      n.kids = std::move(nk);
      moved = 1;
    }
  }
  for (snode& k : n.kids) moved += reorder(k, m, child_of);
  return moved;
}

void mirror(snode& n) {
  std::reverse(n.kids.begin(), n.kids.end());
  for (snode& k : n.kids) mirror(k);
}

/// Σ over events of the depth of the event's top (its outermost touched
/// rank): larger = events rooted deeper = better for saturation. The
/// reversed orientation scores as Σ (n-1 - deepest rank).
std::pair<long long, long long> top_sums(const snode& root,
                                         const xpl::model& m) {
  std::vector<std::uint32_t> order;
  frontier(root, order);
  std::vector<std::uint32_t> rank(m.arity, 0);
  for (std::uint32_t r = 0; r < order.size(); ++r) rank[order[r]] = r;
  long long fwd = 0;
  long long rev = 0;
  const long long n = static_cast<long long>(order.size());
  for (const xpl::event& e : m.events) {
    long long lo = n;
    long long hi = -1;
    const auto span = [&](const SparseBoolArray& sba) {
      for (std::size_t i = 0; i < sba.size(); ++i) {
        const long long r = rank[sba.keyAt(i)];
        lo = std::min(lo, r);
        hi = std::max(hi, r);
      }
    };
    span(e.reads);
    span(e.writes);
    if (hi < 0) continue;
    fwd += lo;
    rev += n - 1 - hi;
  }
  return {fwd, rev};
}

}  // namespace

rewrite_result reorder_force(std::vector<datum> forms, const datum&) {
  const spec s = spec::read(forms);
  if (s.order().empty()) return {std::move(forms), false, "no shape"};
  lia::expr_factory ex;
  const expr_reader reader(ex, s);
  const xpl::model m =
      build_xpl_model(s.order().size(), ex, reader, s, s.events());
  std::size_t moved = 0;
  bool flipped = false;
  long long fwd = 0;
  long long rev = 0;
  for (datum& f : forms) {
    if (!f.is_list() || f.items().empty() || f.head() != "shape") continue;
    snode root = parse_sort(f.items()[1], s);
    std::vector<int> child_of(s.order().size(), -1);
    moved = reorder(root, m, child_of);
    std::tie(fwd, rev) = top_sums(root, m);
    if (rev > fwd) {  // the mirror roots the events deeper
      mirror(root);
      flipped = true;
    }
    f = datum::list({f.items()[0], unparse(root)}, f.line());
  }
  if (moved == 0 && !flipped) {
    return {std::move(forms), false, "FORCE keeps the order at every level"};
  }
  std::ostringstream trace;
  trace << "FORCE reordered " << moved << " shape node"
        << (moved == 1 ? "" : "s") << ", hierarchy kept; polarity "
        << (flipped ? "flipped" : "kept") << " (top depth " << fwd << " vs "
        << rev << " mirrored)";
  return {std::move(forms), true, trace.str()};
}

rewrite_result flatten(std::vector<datum> forms, const datum&) {
  const spec s = spec::read(forms);
  if (s.order().empty()) return {std::move(forms), false, "no shape"};
  for (datum& f : forms) {
    if (!f.is_list() || f.items().empty() || f.head() != "shape") continue;
    if (f.items().size() > 1 && f.items()[1].is_list() &&
        f.items()[1].head() == "spine" &&
        f.items()[1].items().size() == s.order().size() + 1) {
      bool flat = true;
      for (std::size_t i = 1; i < f.items()[1].items().size(); ++i) {
        flat &= f.items()[1].items()[i].is_atom();
      }
      if (flat) return {std::move(forms), false, "already a flat spine"};
    }
    const int line = f.line();
    std::vector<datum> spine{datum::atom("spine", line)};
    for (const std::string& name : s.order()) {
      spine.push_back(datum::atom(name, line));
    }
    f = datum::list({datum::atom("shape", line),
                     datum::list(std::move(spine), line)},
                    line);
  }
  return {std::move(forms), true,
          "the shape is now the flat spine of " +
              std::to_string(s.order().size()) + " leaves"};
}

}  // namespace hsc::surface
