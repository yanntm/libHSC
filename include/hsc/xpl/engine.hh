/// \file engine.hh
/// \brief Enumerative reachability: BFS with may-fire set maintenance,
/// visitor-driven.
///
/// The engine drives a visitor called once per fresh state; the client
/// decides what to keep — the engine's own store exists for termination
/// (the dedup set) and is discarded unless asked for. `explore` is the
/// primitive; `reach` the keep-everything wrapper. Both are functions —
/// no hidden mutable state. Algorithm and the error/cap discipline in
/// `algorithm.md`.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>

#include "hsc/xpl/interpret/model.hh"
#include "hsc/xpl/state.hh"
#include "hsc/xpl/store.hh"

namespace hsc::xpl {

/// The visitor's verdict: keep exploring, or stop the run here.
enum class visit : std::uint8_t { proceed, stop };

/// Called once per fresh state (seeds included), in discovery order. The
/// view dies with the call — the next insert may move the rows; copy to
/// keep. Keep it cheap: it runs inside the search loop.
using state_visitor = std::function<visit(state_id, state_view)>;

struct reach_options {
  /// Bound on stored states — the honest refusal, never a silent
  /// truncation. The default (10⁸) is a memory backstop, not a work
  /// bound: a few million states is normal fare for an explicit engine,
  /// and a driver's timeout is the practical limit.
  std::size_t cap = 100'000'000;
};

struct explore_stats {
  enum class status : std::uint8_t {
    ok,       ///< closed: every reachable state was visited
    stopped,  ///< the visitor said stop: a prefix was visited
    capped,   ///< stopped at the cap: no answer
    error     ///< TOP: a model-level error, diagnostic and witness attached
  };
  status st = status::ok;
  std::size_t count = 0;    ///< states stored (== visited)
  std::uint64_t fired = 0;  ///< successors produced
  std::string diagnostic;   ///< error/capped: the cause
  word witness;             ///< error: the state it happened in
};

/// Close \p seeds under the model's events, reporting each fresh state to
/// \p on_state. Nothing survives the call but what the visitor kept.
/// Every seed must have `m.arity` values; the model must be finalized.
[[nodiscard]] explore_stats explore(const model& m,
                                    std::span<const word> seeds,
                                    const state_visitor& on_state,
                                    const reach_options& opt = {});

/// The keep-everything wrapper: the store is the reachable set.
struct reach_result {
  explore_stats stats;
  state_store states;
};

[[nodiscard]] reach_result reach(const model& m, std::span<const word> seeds,
                                 const reach_options& opt = {});

}  // namespace hsc::xpl
