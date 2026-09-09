/// \file equality.cc
/// \brief The knapsack diagram of a linear equality (`hsc/linear/equality.hh`).
#include "hsc/linear/equality.hh"

#include <map>
#include <unordered_map>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"

namespace hsc::linear {

namespace {

/// For a sort at a position: the words of the subshape by their partial sum,
/// sums above the target pruned (coefficients are nonnegative).
using by_sum = std::map<long long, core::code>;

}  // namespace

core::code equality(core::manager& mgr, core::shape_code top, std::span<const long long> coeff,
                    long long k, const leaf_access& leaves) {
  if (k < 0) return core::none;
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
  span(span, top);
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
        // the values grouped by their weighted contribution, at most k
        const long long c = coeff[first];
        std::map<long long, std::vector<std::int32_t>> groups;
        for (const std::int32_t v : leaves.values(first)) {
          const long long w = c * static_cast<long long>(v);
          if (w <= k) groups[w].push_back(v);
        }
        for (auto& [w, vs] : groups) out[w] = leaves.subset(first, vs);
        break;
      }
      case core::shape_kind::pair: {
        const core::shape_code hs = sh.head(s), ts = sh.tail(s);
        const std::size_t hw = sh.kind(hs) == core::shape_kind::leaf ? 1 : width[hs];
        const by_sum& heads = self(self, hs, first);
        const by_sum& tails = self(self, ts, first + hw);
        for (const auto& [wh, ph] : heads) {
          for (const auto& [wt, pt] : tails) {
            if (wh + wt > k) break;  // tails ascending: nothing further fits
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
  const auto it = all.find(k);
  return it == all.end() ? core::none : it->second;
}

}  // namespace hsc::linear
