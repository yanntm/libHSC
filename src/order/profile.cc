/// \file profile.cc
/// \brief Nodes and arcs per sort of a set (`hsc/order/profile.hh`).
#include "hsc/order/profile.hh"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"

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

}  // namespace hsc::order
