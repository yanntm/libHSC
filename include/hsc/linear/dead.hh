/// \file dead.hh
/// \brief Dead transitions from an over-approximation of the reachable set
/// (`algorithm.md` §2): never enabled on the set, or enabled in one step
/// only from markings that the set says cannot exist.
#pragma once
#include <cstddef>
#include <functional>
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
  std::size_t rounds = 0;      ///< passes of the one-step test over the candidates
  std::size_t candidates = 0;  ///< transitions the one-step test was run on
  bool stopped = false;        ///< the one-step test was cut: the alive ones are alive for want of a test
};

/// The entering predecessors of a slice: for the event \p i, its enabling
/// markings \p en of the set, and the events still \p live (one flag per
/// event), the markings of the set outside \p en from which a live event
/// leads into \p en — `pre(en) ∩ (S ∖ en)` under the live events that write
/// a leaf the guard of \p i reads (`algorithm.md` §2, the backward reading).
/// `none` when there is none. Throws `interrupted` when stopped.
using entry_fn = std::function<core::code(std::size_t i, core::code en, const std::vector<char>& live)>;

/// \brief The verdicts for \p events over \p set (an over-approximation of
/// the reachable set, closed under firing where it is exact), \p init the
/// initial states, \p guards the enabling selector of each event or `none`
/// for untested. \p exact, when not empty, says per event whether its guard
/// is the whole one: the one-step test needs it, "never enabled" does not.
/// With \p entries, the one-step test runs on every candidate (alive, exact,
/// not enabled initially): no entry into its slice from the set means dead;
/// the passes repeat while a transition falls, each with fewer live events.
/// Stopped, the report carries what was decided so far (sound: every
/// verdict stands on tests that completed).
[[nodiscard]] dead_report dead_transitions(core::manager& mgr, core::shape_code top, core::code set,
                                           core::code init, std::span<const core::code> events,
                                           std::span<const core::code> guards,
                                           std::span<const char> exact = {},
                                           const entry_fn& entries = {});

}  // namespace hsc::linear
