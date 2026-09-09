/// \file bandwidth.hh
/// \brief Bandwidth and profile reducing orders over a dependency graph:
/// reverse Cuthill–McKee and Sloan (`README.md`). LTSmin\'s experience
/// (Meijer & van de Pol, "Bandwidth and wavefront reduction for static
/// variable ordering in symbolic reachability analysis", NFM 2016) is that
/// Sloan brings the dependency matrix towards a diagonal and gives good
/// orders for saturation; these are re-implementations, Boost-free, with
/// Boost\'s default Sloan weights.
///
/// Both read an undirected graph over positions `0 .. n-1` given as weighted
/// edges (weights ignored, self-loops ignored), handle every connected
/// component, and return the listing `result[rank] = position`.
#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "hsc/order/louvain/community.h"

namespace hsc::order {

/// Reverse Cuthill–McKee: breadth-first from a pseudo-peripheral node,
/// neighbours by increasing degree, the listing reversed.
[[nodiscard]] std::vector<std::uint32_t> rcm(int n, std::span<const louvain::edge> edges);

/// Sloan: a pseudo-peripheral pair (s, e); priority `w1·dist(v, e) − w2·(deg(v)+1)`
/// over the active front, raised as neighbours are numbered.
[[nodiscard]] std::vector<std::uint32_t> sloan(int n, std::span<const louvain::edge> edges,
                                               int w1 = 1, int w2 = 2);

/// A seeded shuffle of `0 .. n-1`: the control every heuristic must beat.
[[nodiscard]] std::vector<std::uint32_t> random_order(int n, std::uint64_t seed);

}  // namespace hsc::order
