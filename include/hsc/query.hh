/// \file query.hh
/// \brief The cross-level comparison, first shape of the §7 case bracket.
///
/// `x < y` relating two frontier positions across a cut. At the cut that
/// separates them, `split_equiv` the head coordinate and, per class, curry the
/// residual (`v < y`, or `x < w`) onto the tail as a plain per-position
/// restriction. This lives above `core` and `leaves` because §7 is exactly the
/// seam where the calculus consults a leaf theory's `split_equiv`; for now the
/// leaf is `int_set` concretely (the general interface is a later step).
#pragma once

#include <cstddef>

#include "hsc/core/code.hh"

namespace hsc::core {
class manager;
}
namespace hsc::leaves {
class int_set_theory;
}

namespace hsc {

/// \brief The subset of \p diagram (sort \p sort) whose value at frontier
/// position \p xpos is strictly less than its value at \p ypos.
///
/// Both-in-head / both-in-tail descend to the separating cut; there the
/// head-side coordinate must be a leaf (a coordinate deep in the head needs
/// `split_equiv` on diagrams, a later step) and the tail side may be at any
/// depth. Throws `std::logic_error` if the head side is not yet a leaf.
[[nodiscard]] core::code select_less_than(core::manager& mgr,
                                          leaves::int_set_theory& theory,
                                          core::shape_code sort,
                                          core::code diagram, std::size_t xpos,
                                          std::size_t ypos);

}  // namespace hsc
