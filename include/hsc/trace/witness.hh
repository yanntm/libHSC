/// \file witness.hh
/// \brief The witness tree of a CTL verdict: the forward form's set
/// expressions read back as paths, and the backward explanation of what the
/// forward form left to `Sat`. See `algorithm.md` §2–§3.
///
/// Built after the verdict, on demand, from the checker's memoised sets:
/// every segment is a shortest path for its own endpoints, the choices
/// (which target state, which predecessor, which disjunct) are the first
/// found. A universal subformula is reported as holding by exhaustion, with
/// no path.
#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/ctl/checker.hh"
#include "hsc/trace/path.hh"

namespace hsc::trace {

/// One line of the explanation: a state, an event, or a note; `depth` is
/// the nesting of the subformula it belongs to.
struct witness_line {
  enum class kind : std::uint8_t { state, event, note };
  kind what;
  std::size_t depth;
  core::code state = core::none;  ///< `state`: a one-state diagram
  std::size_t event = 0;          ///< `event`: index into the graph's events
  std::string text;               ///< `note`: what holds, or why nothing is shown
};

/// \brief The witness of a converted property whose question tree answered
/// yes (a `TRUE` verdict, or the counterexample of a `FALSE` one — the
/// caller says which through \p negated). Empty when the tree answered no:
/// the property holds by exhaustion and has no path.
std::vector<witness_line> witness(
    core::manager& mgr, ctl::checker& chk, const graph& g,
    const ctl::forward_form& form,
    const std::function<std::string(std::uint32_t)>& atom_name);

}  // namespace hsc::trace
