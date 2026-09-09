/// \file profile.hh
/// \brief The representation size of a set, level by level: for every sort of
/// the shape it reaches, the frontier span, the distinct nodes, their arcs.
///
/// The post-mortem instrument of the ordering work (`README.md`): a belly —
/// node counts rising then falling between two levels — says those levels
/// are correlated, and the shape should bracket what lies between them or
/// bring them together. Read after a reachable set, before a budget is spent.
#pragma once
#include <cstddef>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"

namespace hsc::core {
class manager;
}

namespace hsc::order {

struct level_profile {
  core::shape_code sort = core::none;
  std::size_t first = 0;  ///< first frontier position the sort spans
  std::size_t width = 0;  ///< how many leaves it spans
  std::size_t nodes = 0;  ///< distinct diagram nodes of that sort in the set
  std::size_t arcs = 0;   ///< their arcs, summed
};

/// \brief The profile of \p set under the shape rooted at \p top, in frontier
/// order (a wider sort before the narrower ones opening at the same leaf).
[[nodiscard]] std::vector<level_profile> profile(core::manager& mgr, core::shape_code top,
                                                 core::code set);

}  // namespace hsc::order
