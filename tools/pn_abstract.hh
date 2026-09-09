/// \file pn_abstract.hh
/// \brief The abstraction of a net by the places it keeps: the others are
/// removed with their arcs. Removing a place only adds behaviour (a
/// precondition gone, a postcondition nobody reads), so the reachable set of
/// the abstract net projects over that of the original; what is impossible
/// in the abstract net is impossible in the original, for every question
/// that reads kept places only.
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "hsc/petri/core/SparsePetriNet.h"

namespace hsc::pn {

struct abstraction {
  SparsePetriNet<int> net;                ///< the kept places, every transition
  std::vector<std::size_t> to_abstract;   ///< original place → abstract index, or SIZE_MAX when removed
  std::vector<std::size_t> kept;          ///< abstract index → original place
  std::size_t removed = 0;
};

/// The net \p src without the places \p keep marks false. Names are kept.
inline abstraction abstract_net(const SparsePetriNet<int>& src, const std::vector<char>& keep) {
  abstraction a;
  const std::size_t np = src.getPlaceCount(), nt = src.getTransitionCount();
  a.to_abstract.assign(np, static_cast<std::size_t>(-1));
  for (std::size_t p = 0; p < np; ++p) {
    if (!keep[p]) { ++a.removed; continue; }
    a.to_abstract[p] = a.net.addPlace(src.getPnames()[p], src.getMarks()[p]);
    a.kept.push_back(p);
  }
  for (std::size_t t = 0; t < nt; ++t) {
    const std::size_t at = a.net.addTransition(src.getTnames()[t]);
    const SparseArray<int>& pre = src.getFlowPT().getColumn(t);
    for (std::size_t k = 0; k < pre.size(); ++k) {
      const std::size_t p = pre.keyAt(k);
      if (keep[p]) a.net.addPreArc(static_cast<int>(a.to_abstract[p]), static_cast<int>(at), pre.valueAt(k));
    }
    const SparseArray<int>& post = src.getFlowTP().getColumn(t);
    for (std::size_t k = 0; k < post.size(); ++k) {
      const std::size_t p = post.keyAt(k);
      if (keep[p]) a.net.addPostArc(static_cast<int>(a.to_abstract[p]), static_cast<int>(at), post.valueAt(k));
    }
  }
  a.net.setName(src.getName());
  return a;
}

}  // namespace hsc::pn
