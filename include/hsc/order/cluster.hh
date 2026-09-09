/// \file cluster.hh
/// \brief A hierarchy over positions from a weighted dependency graph, with
/// two ways for invariants to take part: signed supports as cliques, and
/// groups contracted into one node before the clustering (`README.md`).
///
/// Generic: positions are integers, edges are the caller\'s reading of its
/// model (a net\'s transitions, a spec\'s events), and the result is nested
/// groups of positions the caller turns into its own tree (a NUPN unit
/// tree, `(balanced …)` blocks). Louvain does the clustering.
#pragma once
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "hsc/order/louvain/community.h"

namespace hsc::order {

/// The knobs of the invariant edges, read once from the environment:
/// `HSC_INV_WEIGHT` (a support\'s clique weight, shared over its pairs, default
/// 1), `HSC_INV_CROSS` (the fraction of it a pair of opposite signs gets,
/// default 0.5), `HSC_INV_MERGE` (the constant from which a support is
/// contracted into one flat node, default 0: never).
struct invariant_knobs {
  double weight = 1.0;
  double cross = 0.5;
  long long merge = 0;
  [[nodiscard]] static const invariant_knobs& from_env();
};

/// A signed support with the constant it conserves: `Σ sign·… = constant`.
struct signed_support {
  std::vector<std::pair<int, int>> members;  ///< (position, coefficient)
  long long constant = 0;
};

/// \brief The nested groups: `members[n]` are the positions of node `n` (a
/// contraction class, or one position), `kids[L][c]` the level-(L-1) nodes
/// (nodes when L == 0) in community `c` of level `L`. Empty `kids` when the
/// graph is empty.
struct hierarchy {
  std::vector<std::vector<int>> members;
  std::vector<std::vector<std::vector<int>>> kids;
  [[nodiscard]] int top() const noexcept { return static_cast<int>(kids.size()) - 1; }
};

/// \brief Cluster \p positions positions: the supports whose constant reaches
/// the merge knob are contracted (union of overlapping supports), every
/// support adds its sign-aware clique over the contracted nodes (bounded like
/// any hyperedge, `louvain/hyperedge.hh`), and Louvain levels the rest.
[[nodiscard]] hierarchy cluster(int positions, std::vector<louvain::edge> edges,
                                std::span<const signed_support> invariants,
                                const invariant_knobs& knobs = invariant_knobs::from_env());

}  // namespace hsc::order
