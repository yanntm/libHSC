/// \file profile.cc
/// \brief Nodes and arcs per sort of a set (`hsc/order/profile.hh`).
#include "hsc/order/profile.hh"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/core/support.hh"

namespace hsc::order {

std::vector<level_profile> profile(core::manager& mgr, core::shape_code top, core::code set) {
  const core::shape_table& sh = mgr.shapes();
  // The span of every sort of the shape.
  std::unordered_map<core::shape_code, std::pair<std::size_t, std::size_t>> span;
  std::size_t next = 0;
  const auto walk = [&](auto&& self, core::shape_code s) -> std::pair<std::size_t, std::size_t> {
    if (sh.kind(s) == core::shape_kind::leaf) {
      const auto r = std::make_pair(next++, std::size_t{1});
      span[s] = r;
      return r;
    }
    if (sh.kind(s) == core::shape_kind::unit) return {next, 0};
    const auto h = self(self, sh.head(s));
    const auto t = self(self, sh.tail(s));
    const auto r = std::make_pair(h.first, h.second + t.second);
    span[s] = r;
    return r;
  };
  walk(walk, top);
  // Distinct nodes and arcs per sort, by one traversal of the set.
  std::unordered_map<core::shape_code, level_profile> counts;
  std::unordered_set<core::code> seen;
  std::vector<core::shape_code> order;
  const core::diagram_engine& d = mgr.diagrams();
  const auto visit = [&](auto&& self, core::code n) -> void {
    if (n == core::none || !seen.insert(n).second) return;
    const core::shape_code s = d.sort_of(n);
    if (sh.kind(s) != core::shape_kind::pair) return;
    level_profile& k = counts[s];
    if (k.nodes == 0) {
      order.push_back(s);
      k.sort = s;
      k.first = span[s].first;
      k.width = span[s].second;
    }
    ++k.nodes;
    const std::span<const core::arc> arcs = d.arcs(n);
    k.arcs += arcs.size();
    const bool head_node = sh.kind(sh.head(s)) == core::shape_kind::pair;
    const bool tail_node = sh.kind(sh.tail(s)) == core::shape_kind::pair;
    for (const core::arc& a : arcs) {
      if (head_node) self(self, a.prime);
      if (tail_node) self(self, a.sub);
    }
  };
  visit(visit, set);
  std::ranges::sort(order, [&](core::shape_code a, core::shape_code b) {
    const level_profile& x = counts[a];
    const level_profile& y = counts[b];
    return x.first != y.first ? x.first < y.first : x.width > y.width;
  });
  std::vector<level_profile> out;
  out.reserve(order.size());
  for (const core::shape_code s : order) out.push_back(counts[s]);
  return out;
}

namespace {

/// The span of every sort and the leaf sort at every frontier position.
struct spans {
  std::unordered_map<core::shape_code, std::pair<std::size_t, std::size_t>> of;
  std::vector<core::shape_code> leaf_at;
};

spans walk_spans(const core::shape_table& sh, core::shape_code top) {
  spans sp;
  const auto walk = [&](auto&& self, core::shape_code s) -> std::pair<std::size_t, std::size_t> {
    if (sh.kind(s) == core::shape_kind::leaf) {
      sp.leaf_at.push_back(s);
      const auto r = std::make_pair(sp.leaf_at.size() - 1, std::size_t{1});
      sp.of[s] = r;
      return r;
    }
    if (sh.kind(s) == core::shape_kind::unit) return {sp.leaf_at.size(), 0};
    const auto h = self(self, sh.head(s));
    const auto t = self(self, sh.tail(s));
    const auto r = std::make_pair(h.first, h.second + t.second);
    sp.of[s] = r;
    return r;
  };
  walk(walk, top);
  return sp;
}

/// Every distinct node of \p set, grouped by sort, in first-visit order.
std::vector<std::pair<core::shape_code, std::vector<core::code>>> nodes_by_sort(
    core::manager& mgr, core::code set) {
  const core::shape_table& sh = mgr.shapes();
  const core::diagram_engine& d = mgr.diagrams();
  std::unordered_map<core::shape_code, std::size_t> slot;
  std::vector<std::pair<core::shape_code, std::vector<core::code>>> out;
  std::unordered_set<core::code> seen;
  const auto visit = [&](auto&& self, core::code n) -> void {
    if (n == core::none || !seen.insert(n).second) return;
    const core::shape_code s = d.sort_of(n);
    if (sh.kind(s) != core::shape_kind::pair) return;
    const auto it = slot.find(s);
    if (it == slot.end()) {
      slot[s] = out.size();
      out.emplace_back(s, std::vector<core::code>{n});
    } else {
      out[it->second].second.push_back(n);
    }
    const bool head_node = sh.kind(sh.head(s)) == core::shape_kind::pair;
    const bool tail_node = sh.kind(sh.tail(s)) == core::shape_kind::pair;
    for (const core::arc& a : d.arcs(n)) {
      if (head_node) self(self, a.prime);
      if (tail_node) self(self, a.sub);
    }
  };
  visit(visit, set);
  return out;
}

}  // namespace

std::vector<local_states> subshape_states(core::manager& mgr, core::shape_code top, core::code set) {
  const spans sp = walk_spans(mgr.shapes(), top);
  core::diagram_engine& d = mgr.diagrams();
  std::vector<local_states> out;
  for (const auto& [sort, nodes] : nodes_by_sort(mgr, set)) {
    core::code u = core::none;
    for (const core::code n : nodes) u = u == core::none ? n : d.join(u, n);
    const auto [first, width] = sp.of.at(sort);
    out.push_back({sort, first, width, u == core::none ? 0.0 : d.cardinal(u)});
  }
  std::ranges::sort(out, [](const local_states& a, const local_states& b) {
    return a.first != b.first ? a.first < b.first : a.width > b.width;
  });
  return out;
}

std::vector<leaf_domain> leaf_domains(core::manager& mgr, core::shape_code top, core::code set) {
  const spans sp = walk_spans(mgr.shapes(), top);
  const core::shape_table& sh = mgr.shapes();
  const core::diagram_engine& d = mgr.diagrams();
  // the union of the primes of every node whose head is a leaf, per leaf
  std::unordered_map<core::shape_code, core::code> dom;
  for (const auto& [sort, nodes] : nodes_by_sort(mgr, set)) {
    const core::shape_code head = sh.head(sort);
    if (sh.kind(head) != core::shape_kind::leaf) continue;
    core::support_algebra& alg = mgr.algebra(head);
    core::code u = dom.count(head) ? dom[head] : core::none;
    for (const core::code n : nodes)
      for (const core::arc& a : d.arcs(n)) u = u == core::none ? a.prime : alg.join(u, a.prime);
    dom[head] = u;
  }
  std::vector<leaf_domain> out;
  for (std::size_t p = 0; p < sp.leaf_at.size(); ++p) {
    const core::shape_code leaf = sp.leaf_at[p];
    const auto it = dom.find(leaf);
    out.push_back({p, it == dom.end() || it->second == core::none ? 0.0 : mgr.algebra(leaf).cardinal(it->second)});
  }
  return out;
}

}  // namespace hsc::order
