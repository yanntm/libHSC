/// \file invariants.hh
/// \brief The place invariants (P-flows) of a net, from PetriSpot's vendored
/// calculator, as supports with coefficients and the constant the initial
/// marking fixes.
///
/// A flow `Σ c_p · m(p) = K` ties its support together in every reachable
/// marking: what the decomposition wants to know (`decompose.hh`), and what
/// the surface could one day declare as a constraint. Flows, not semiflows: a
/// basis has at most |P| of them where the positive ones can be exponentially
/// many, and a flow with mixed signs still says its places move together. The
/// calculator runs with a deadline and without compression, so the basis comes
/// back plain; an arithmetic overflow or the deadline yields what was found
/// (possibly nothing), never an error.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>

#include "hsc/petri/core/SparsePetriNet.h"

namespace hsc::petri {

/// One flow: `Σ coeff · m(place) = constant` on every reachable marking.
struct pflow {
  std::vector<std::pair<int, int>> terms;  ///< (place index, coefficient), ascending
  long long constant = 0;                   ///< its value at the initial marking
};

/// \brief A basis of the flows of \p net, when the calculator finishes within
/// \p seconds (a partial elimination is no basis: nothing then). With
/// \p positive, the semiflows instead — those found by the deadline. Empty
/// when the net has none, or on overflow.
[[nodiscard]] std::vector<pflow> pflows(const SparsePetriNet<int>& net, int seconds,
                                        bool positive = false);

}  // namespace hsc::petri
