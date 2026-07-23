/// \file decompose.hh
/// \brief Synthesise a NUPN unit tree for a bare net by community detection.
///
/// When a PN arrives with no unit tree, the importer would fall back to a flat
/// spine. This computes a hierarchy instead: Louvain modularity clustering over
/// the place co-occurrence graph (two places are linked when a transition
/// touches both). The graph build follows PetriSpot/gal's `GraphBuilder`; the
/// Louvain method is ours (the Java shells out to an external binary). The
/// result is an ordinary `unit_tree`, so the rest of the chain is unchanged and
/// HSC never learns a decomposition happened — §8's shape choice, upstream.
#pragma once

#include "hsc/petri/SparsePetriNet.h"
#include "hsc/petri/nupn.hh"

namespace hsc::petri {

/// \brief A unit tree grouping \p net's places by Louvain communities, nested by
/// aggregation level. Degrades to a flat grouping when the graph has no
/// community structure. Unit ids are generated (`u0`, `u1`, …).
[[nodiscard]] unit_tree decompose(const SparsePetriNet<int>& net);

}  // namespace hsc::petri
