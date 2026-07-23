/// \file query.hh
/// \brief Cross-level comparison of two coordinates, first shape of the §7
/// case bracket.
///
/// `x ⋈ y` (⋈ one of < ≤ = ≠ ≥ >) relating two frontier positions across a
/// cut. At the cut that separates them, `split_equiv` the head coordinate and,
/// per class, curry the residual (`v ⋈ y`, or `x ⋈ v`) onto the tail as a
/// plain per-position restriction. This lives above `core` and `leaves`
/// because §7 is exactly the seam where the calculus consults a leaf theory's
/// `split_equiv`; for now the leaf is `int_set` concretely (the general
/// interface is a later step).
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "hsc/core/code.hh"

namespace hsc::core {
class manager;
}
namespace hsc::leaves {
class int_set_theory;
}

namespace hsc {

/// \brief The comparison relating the two coordinates, read `x op y`.
enum class cmp : std::uint8_t { lt, le, eq, ne, ge, gt };

/// \brief `l op r` on ground values — the residual once both sides are known.
[[nodiscard]] constexpr bool eval(cmp op, std::int32_t l,
                                  std::int32_t r) noexcept {
  switch (op) {
    case cmp::lt: return l < r;
    case cmp::le: return l <= r;
    case cmp::eq: return l == r;
    case cmp::ne: return l != r;
    case cmp::ge: return l >= r;
    case cmp::gt: return l > r;
  }
  return false;
}

/// \brief Partition \p diagram (sort \p sort) by the value at frontier
/// position \p pos — `split_equiv` lifted from the leaf to diagrams.
///
/// One class per distinct value, ascending; each class is the subset of
/// \p diagram holding that value at \p pos. A leaf is the theory's own split;
/// a pair splits the side holding \p pos one level in and merges classes by
/// common value. Exact, like the leaf: no cost knob.
[[nodiscard]] std::vector<std::pair<std::int32_t, core::code>> split_equiv(
    core::manager& mgr, leaves::int_set_theory& theory, core::shape_code sort,
    core::code diagram, std::size_t pos);

/// \brief The subset of \p diagram (sort \p sort) whose value at frontier
/// position \p xpos stands in relation \p op to its value at \p ypos.
///
/// `xpos == ypos` resolves on reflexivity alone (`x op x` is all or nothing).
/// Otherwise both-in-head / both-in-tail descend to the separating cut; there
/// the head side is split by its coordinate at whatever depth (`split_equiv`)
/// and each class curries the residual onto the tail. The positions may land
/// anywhere in the shape.
[[nodiscard]] core::code select_compare(core::manager& mgr,
                                        leaves::int_set_theory& theory,
                                        core::shape_code sort,
                                        core::code diagram, std::size_t xpos,
                                        cmp op, std::size_t ypos);

/// \brief The subset of \p diagram whose value at frontier position \p pos
/// lies in the theory set \p set. The separable one-position criterion: a
/// plain descent to that leaf followed by a meet, no split involved.
[[nodiscard]] core::code select_in(core::manager& mgr,
                                   leaves::int_set_theory& theory,
                                   core::shape_code sort, core::code diagram,
                                   std::size_t pos, core::code set);

}  // namespace hsc
