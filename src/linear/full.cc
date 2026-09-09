/// \file full.cc
/// \brief The product of the leaf domains (`hsc/linear/full.hh`).
#include "hsc/linear/full.hh"

#include <unordered_map>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"

namespace hsc::linear {

core::code full_box(core::manager& mgr, core::shape_code top,
                    const std::function<core::code(std::size_t)>& domain) {
  const core::shape_table& sh = mgr.shapes();
  core::diagram_engine& d = mgr.diagrams();
  // widths, so the tail's first position is known at every node
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
  // the product, memoised per (sort, first position): equal subshapes at
  // different positions have different domains
  std::unordered_map<std::uint64_t, core::code> memo;
  const auto build = [&](auto&& self, core::shape_code s, std::size_t first) -> core::code {
    switch (sh.kind(s)) {
      case core::shape_kind::unit:
        return d.one();
      case core::shape_kind::leaf:
        return domain(first);
      case core::shape_kind::pair: {
        const std::uint64_t key = (static_cast<std::uint64_t>(s) << 24) ^ first;
        if (const auto it = memo.find(key); it != memo.end()) return it->second;
        const core::shape_code hs = sh.head(s), ts = sh.tail(s);
        const std::size_t hw = sh.kind(hs) == core::shape_kind::leaf ? 1 : width[hs];
        const core::code prime = self(self, hs, first);
        const core::code sub = self(self, ts, first + hw);
        const core::code r = (prime == core::none || sub == core::none) ? core::none : d.rectangle(s, prime, sub);
        memo[key] = r;
        return r;
      }
    }
    return core::none;
  };
  return build(build, top, 0);
}

}  // namespace hsc::linear
