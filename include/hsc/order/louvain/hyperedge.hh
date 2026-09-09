/// \file hyperedge.hh
/// \brief What a single hyperedge may contribute to the graph handed to Louvain.
///
/// Clustering sees a graph, but a net or a spec offers hyperedges: one event
/// touches a whole set of positions at once. Relating that set pairwise costs
/// `k(k-1)/2` edges for a support of `k`, `|ctrl| * |write|` for the flow-like
/// split, so a single wide event can outweigh every other edge in the graph.
///
/// It also says little. An event holding many control positions is a
/// synchronisation -- the point where otherwise independent components meet --
/// not the progression of one of them, and the clique it induces links places
/// that share nothing but that meeting. Beyond these bounds the event is left
/// out of the graph rather than expanded: the communities are read from the
/// events that speak of locality.
#pragma once

#include <cstddef>

namespace hsc::petri::louvain {

/// Control positions above which an event reads as a synchronisation.
inline constexpr std::size_t max_control = 8;

/// Edges one event may induce, whichever strategy builds them.
inline constexpr std::size_t max_induced = 64;

/// \brief Whether an event inducing \p induced edges is worth expanding.
[[nodiscard]] constexpr bool induced_fits(std::size_t induced) noexcept {
  return induced != 0 && induced <= max_induced;
}

/// \brief Whether an event reading \p ctrl positions is a progression, not a
/// synchronisation.
[[nodiscard]] constexpr bool control_fits(std::size_t ctrl) noexcept {
  return ctrl <= max_control;
}

}  // namespace hsc::petri::louvain
