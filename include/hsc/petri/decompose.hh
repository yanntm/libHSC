/// \file decompose.hh
/// \brief Synthesise a NUPN unit tree for a bare net by community detection.
///
/// When a PN arrives with no unit tree, the importer would fall back to a flat
/// spine. This computes a hierarchy instead: Louvain modularity clustering over
/// the place co-occurrence graph (two places are linked when a transition
/// touches both). The graph build follows PetriSpot/gal's `GraphBuilder`; the
/// Louvain method is ours (the Java shells out to an external binary). The
/// result is an ordinary `unit_tree`, so the rest of the chain is unchanged and
/// HSC never learns a decomposition happened — the shape choice, made upstream.
#pragma once
#include <cstdint>
#include <span>

#include "hsc/order/louvain/community.h"
#include "hsc/petri/invariants.hh"

#include "hsc/petri/core/SparsePetriNet.h"
#include "hsc/petri/nupn.hh"

namespace hsc::petri {

/// \brief A unit tree grouping \p net's places by Louvain communities, nested by
/// aggregation level. Degrades to a flat grouping when the graph has no
/// community structure. Unit ids are generated (`u0`, `u1`, …).
/// \p invariants, when given, take part in the graph: each flow's support is
/// one more hyperedge (a clique, weight shared over its pairs, bounded like
/// a transition's), so places an invariant ties together attract each other.
[[nodiscard]] unit_tree decompose(const SparsePetriNet<int>& net,
                                  std::span<const pflow> invariants = {});

/// The place dependency graph the clustering sees (control→write per
/// transition, all-to-all as fallback), for the orderings of
/// `order/bandwidth.hh`.
[[nodiscard]] std::vector<order::louvain::edge> dependency_edges(const SparsePetriNet<int>& net);

/// A flat unit tree: the places in the order \p listing gives (`listing[rank]`
/// is a place index).
[[nodiscard]] unit_tree ordered(const SparsePetriNet<int>& net, std::span<const std::uint32_t> listing);

}  // namespace hsc::petri
