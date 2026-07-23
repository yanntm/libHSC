/// \file force.hh
/// \brief The FORCE variable-ordering heuristic (Aloul-Markov-Sakallah
/// 2003), re-expressed from libITS `its/gal/force.cpp`. See README.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace hsc::order {

/// Variables one event touches, to be kept close. Cost: the span of the
/// members in the order, times the weight.
struct clique {
  std::vector<std::uint32_t> vars;
  float weight = 1.0f;
};

/// An asymmetric preference `before < after`: free when the order
/// satisfies it, the distance times the weight when it does not.
struct precedence {
  std::uint32_t before;
  std::uint32_t after;
  float weight = 1.0f;
};

/// \brief Compute an order for variables `0 .. nvars-1` minimizing the
/// summed constraint cost, starting from the identity.
///
/// Deterministic. Returns the new frontier listing: `result[rank]` is the
/// original variable at that rank — a permutation of `0 .. nvars-1`.
/// Variables no constraint mentions keep their relative place. At most
/// `max_iters` spring iterations (0: the libITS default, `nvars` capped
/// at 256); the cheapest order seen wins, so more iterations never hurt
/// the result.
[[nodiscard]] std::vector<std::uint32_t> force(
    std::size_t nvars, std::span<const clique> cliques,
    std::span<const precedence> precedences, std::size_t max_iters = 0);

}  // namespace hsc::order
