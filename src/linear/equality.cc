/// \file equality.cc
/// \brief The knapsack diagram of a linear equality (`hsc/linear/equality.hh`).
#include "hsc/linear/equality.hh"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"

namespace hsc::linear {

namespace {

/// For a sort at a position: the words of the subshape by their partial sum,
/// sums above the target pruned (coefficients are nonnegative).
using by_sum = std::map<long long, core::code>;

}  // namespace

core::code equality(core::manager& mgr, core::shape_code top, std::span<const long long> coeff,
                    long long k, const leaf_access& leaves, bool at_most) {
  const core::shape_table& sh = mgr.shapes();
  core::diagram_engine& d = mgr.diagrams();
  std::unordered_map<core::shape_code, std::size_t> width;
  const auto span = [&](auto&& self, core::shape_code s) -> std::size_t {
    switch (sh.kind(s)) {
      case core::shape_kind::unit: return 0;
      case core::shape_kind::leaf: return 1;
      case core::shape_kind::pair: {
        if (const auto it = width.find(s); it != width.end()) return it->second;
        const std::size_t w = self(self, sh.head(s)) + self(self, sh.tail(s));
        width[s] = w;
        return w;
      }
    }
    return 0;
  };
  const std::size_t total = span(span, top);
  // The range of each position's contribution, and the prefix sums of the
  // ranges: a partial sum over the positions [first, first+w) is feasible
  // iff the complement can supply the rest — k − x within the complement's
  // range (for `at_most`: x plus the complement's least at most k). This is
  // the pruning; with nonnegative coefficients it is `x ≤ k`.
  std::vector<long long> lo(total, 0), hi(total, 0);
  for (std::size_t i = 0; i < total; ++i) {
    bool first_value = true;
    for (const std::int32_t v : leaves.values(i)) {
      const long long w = coeff[i] * static_cast<long long>(v);
      if (first_value) { lo[i] = hi[i] = w; first_value = false; }
      else { lo[i] = std::min(lo[i], w); hi[i] = std::max(hi[i], w); }
    }
  }
  std::vector<long long> plo(total + 1, 0), phi(total + 1, 0);
  for (std::size_t i = 0; i < total; ++i) { plo[i + 1] = plo[i] + lo[i]; phi[i + 1] = phi[i] + hi[i]; }
  const auto feasible = [&](long long x, std::size_t first, std::size_t w) {
    const long long cmin = plo[first] + (plo[total] - plo[first + w]);
    const long long cmax = phi[first] + (phi[total] - phi[first + w]);
    return at_most ? x + cmin <= k : (k - x >= cmin && k - x <= cmax);
  };
  std::unordered_map<std::uint64_t, by_sum> memo;  // (sort, first position)
  const auto sums = [&](auto&& self, core::shape_code s, std::size_t first) -> const by_sum& {
    const std::uint64_t key = (static_cast<std::uint64_t>(s) << 24) ^ first;
    if (const auto it = memo.find(key); it != memo.end()) return it->second;
    by_sum out;
    switch (sh.kind(s)) {
      case core::shape_kind::unit:
        out[0] = d.one();
        break;
      case core::shape_kind::leaf: {
        // the values grouped by their weighted contribution, the infeasible dropped
        const long long c = coeff[first];
        std::map<long long, std::vector<std::int32_t>> groups;
        for (const std::int32_t v : leaves.values(first)) {
          const long long w = c * static_cast<long long>(v);
          if (feasible(w, first, 1)) groups[w].push_back(v);
        }
        for (auto& [w, vs] : groups) out[w] = leaves.subset(first, vs);
        break;
      }
      case core::shape_kind::pair: {
        const core::shape_code hs = sh.head(s), ts = sh.tail(s);
        const std::size_t hw = sh.kind(hs) == core::shape_kind::leaf ? 1 : width[hs];
        const std::size_t w = width[s];
        const by_sum& heads = self(self, hs, first);
        const by_sum& tails = self(self, ts, first + hw);
        for (const auto& [wh, ph] : heads) {
          for (const auto& [wt, pt] : tails) {
            if (!feasible(wh + wt, first, w)) continue;
            const core::code r = d.rectangle(s, ph, pt);
            if (r == core::none) continue;
            auto [it, fresh] = out.try_emplace(wh + wt, r);
            if (!fresh) it->second = d.join(it->second, r);
          }
        }
        break;
      }
    }
    return memo.emplace(key, std::move(out)).first->second;
  };
  const by_sum& all = sums(sums, top, 0);
  if (!at_most) {
    const auto it = all.find(k);
    return it == all.end() ? core::none : it->second;
  }
  core::code r = core::none;  // every residual sum ≤ k (the map holds none above)
  for (const auto& [w, c] : all) r = r == core::none ? c : d.join(r, c);
  return r;
}

}  // namespace hsc::linear
