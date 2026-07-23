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

/// \brief The subset of \p diagram (sort \p sort) whose value at frontier
/// position \p xpos stands in relation \p op to its value at \p ypos.
///
/// `xpos == ypos` resolves on reflexivity alone (`x op x` is all or nothing).
/// Otherwise both-in-head / both-in-tail descend to the separating cut; there
/// the head-side coordinate must be a leaf (a coordinate deep in the head
/// needs `split_equiv` on diagrams, a later step) and the tail side may be at
/// any depth. Throws `std::logic_error` if the head side is not yet a leaf.
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
