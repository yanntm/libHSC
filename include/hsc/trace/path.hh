/// \file path.hh
/// \brief A shortest path between two sets through the reachability graph,
/// as states and event indices. See `algorithm.md` §1.
///
/// Symbolic in both phases: forward layers until a target is reached (the
/// test existential), then a backtrack one state at a time through the
/// inverted events applied to one-state diagrams. The caller says how to
/// pick one state out of a set (a theory-level choice this package does not
/// make itself) and supplies the inverted events when it has them.
#pragma once
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"

namespace hsc::core {
class manager;
}

namespace hsc::trace {

/// A run: `states[0] → events[0] → states[1] → … → states[k]`.
struct path_result {
  std::vector<core::code> states;      ///< one-state diagrams, k+1 of them
  std::vector<std::size_t> events;     ///< indices into the event list, k of them
};

/// What a search runs over.
struct graph {
  core::shape_code sort = core::none;
  std::span<const core::code> events;  ///< forward event terms, in order
  std::span<const core::code> preds;   ///< their converses in the same order; empty when unavailable
  core::code within = core::none;      ///< the reachable set, `none` for unrestricted
  /// One state of a nonempty set, as a one-state diagram.
  std::function<core::code(core::code)> one_state;
};

/// \brief A shortest run from a state of \p from to a state of \p to whose
/// intermediate states satisfy \p constraint (a selector term; `id` for
/// none). `nullopt` when no such run exists. A run of length 0 when the two
/// sets meet.
std::optional<path_result> path(core::manager& mgr, const graph& g,
                                core::code from, core::code to,
                                core::code constraint,
                                core::code through = core::none);
/// \p through, when not `none`, is a *set* the intermediate states must lie
/// in (a constraint given as data — a `Sat` — rather than as a selector).

}  // namespace hsc::trace
