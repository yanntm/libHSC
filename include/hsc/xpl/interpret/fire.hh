/// \file fire.hh
/// \brief Firing one event at one state: zero or more successors, zero
/// meaning not actually enabled.
///
/// Semantics in `algorithm.md` §2: the term interprets over branches —
/// filters test the branch state where they stand, updates write
/// simultaneously, havoc and `alt` fork, `abort` kills. Errors carry the
/// event, the cause, and the state they happened in.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "hsc/xpl/interpret/model.hh"
#include "hsc/xpl/state.hh"

namespace hsc::xpl {

/// A model-level runtime error: ⊥ at a decision, overflow, conflicting
/// writes. The engine turns it into the TOP result.
class interp_error : public std::runtime_error {
 public:
  interp_error(std::uint32_t event, const std::string& cause, word at)
      : std::runtime_error(cause), event_(event), at_(std::move(at)) {}
  [[nodiscard]] std::uint32_t event() const { return event_; }
  [[nodiscard]] const word& at() const { return at_; }

 private:
  std::uint32_t event_;
  word at_;  ///< the state the error happened in
};

/// One successor: the state, and the positions whose value differs from
/// the source (by value comparison — a restoring write is not a change).
struct successor {
  word s;
  std::vector<std::uint32_t> changed;
};

/// The may-fire test: `events[e].quick` at \p s. A necessary condition
/// only — `fire` is authoritative. Deciding on ⊥ throws.
[[nodiscard]] bool quick_enabled(const model& m, std::uint32_t e,
                                 state_view s);

/// Append every successor of \p s by `events[e]` to \p out. May append
/// nothing: the event was not actually enabled.
void fire(const model& m, std::uint32_t e, state_view s,
          std::vector<successor>& out);

}  // namespace hsc::xpl
