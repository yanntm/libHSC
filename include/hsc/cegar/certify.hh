/// \file cegar/certify.hh
/// \brief Certification: the leaf-local inclusion check L(lts) ⊆ L(H).
///
/// BFS on pairs (state, class) following defined leaf transitions. A
/// reachable pair with a dead class exhibits a leaf trace the classifier
/// rejects — returned as a positive counterexample; no such pair proves
/// the inclusion, and the walk itself is the certificate.

#pragma once

#include "hsc/cegar/classifier.hh"
#include "hsc/cegar/model.hh"

namespace hsc::cegar {

struct cert_result {
  bool certified = false;
  /// Set iff not certified: a word in L(lts) \ L(H).
  word counterexample;
};

/// Cost O(|Q| * k * |Σ|), leaf-local.
[[nodiscard]] cert_result certify(const lts& l, const classifier& h);

}  // namespace hsc::cegar
