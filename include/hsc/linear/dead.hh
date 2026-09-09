/// \file dead.hh
/// \brief Dead transitions from an over-approximation of the reachable set
/// (`algorithm.md` §2): never enabled on the set, or enabled in one step
/// only from markings that the set says cannot exist.
#pragma once
#include <cstddef>
#include <span>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"

namespace hsc::core {
class manager;
}

namespace hsc::linear {

/// One verdict per event, in the events' order.
enum class verdict : unsigned char {
  untested,       ///< no guard selector given (a family, a no-op, an uncovered place)
  never_enabled,  ///< the guard selects nothing of the set
  one_step,       ///< disabled initially, and no successor of a disabled marking of the set enables it
  alive,          ///< the set does not rule it out
};

struct dead_report {
  std::vector<verdict> verdicts;
  std::size_t never_enabled = 0, one_step = 0, alive = 0, untested = 0;
};

/// \brief The verdicts for \p events over \p set (an over-approximation of
/// the reachable set, closed under firing where it is exact), \p init the
/// initial states, \p guards the enabling selector of each event or `none`
/// for untested. \p step is the sum of the events (one image of the set is
/// computed once); with \p sharpen, a transition the cheap test leaves alive
/// gets the exact one (`next(set ∖ en(t))`, one image each) while their count
/// is at most \p sharpen.
/// With \p with_step false only the first test runs (no image of the set).
[[nodiscard]] dead_report dead_transitions(core::manager& mgr, core::shape_code top, core::code set,
                                           core::code init, std::span<const core::code> events,
                                           std::span<const core::code> guards, core::code step,
                                           bool with_step = true, std::size_t sharpen = 0);

}  // namespace hsc::linear
