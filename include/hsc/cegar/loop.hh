/// \file cegar/loop.hh
/// \brief The verification loop: abstract-product search over certified
/// classifiers, shape-descent replay, refine-until-resolved; verdicts
/// carry their evidence (a replayable witness, or a certificate).
///
/// Leaves are interned by their serialized bytes: byte-equal leaves share
/// one learner, one classifier, one certification and one budget. All
/// policy knobs preserve soundness and the budget; they exist to be
/// measured.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hsc/cegar/classifier.hh"
#include "hsc/cegar/model.hh"

namespace hsc::cegar {

struct options {
  enum class culprits { all, first, cheapest };
  culprits pick = culprits::all;
  /// Drive every refined leaf straight to its exact rung.
  bool jump_exact = false;
  /// Share learners across byte-equal leaves (off only for ablation).
  bool intern = true;
  /// Cap on abstract states materialized per search round.
  std::int64_t cap = 1'000'000;
};

struct run_result {
  verdict v;
  /// Loop statistics, over distinct leaves where per-leaf.
  std::int64_t rounds = 0;
  std::int64_t cex_total = 0;
  std::int64_t budget = 0;  ///< Σ (|Q_i|+1) over distinct leaves.
  std::int32_t leaves_chaotic = 0, leaves_intermediate = 0,
               leaves_exact = 0;
  std::int64_t inv_size = 0;  ///< |Inv| when v.k == holds.
  /// The certificate text (.cert) when v.k == holds, else empty.
  std::string certificate;
};

/// Algorithm 4.1 of the paper on model m. Round assertions and the
/// budget cap are enforced with hard aborts — a failure is a bug in the
/// learner or the teacher, not a condition to handle.
[[nodiscard]] run_result run(const model& m, const options& opt);

}  // namespace hsc::cegar
