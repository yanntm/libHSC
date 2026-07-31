/// \file cegar/loop.hh
/// \brief The verification loop: abstract-product search over certified
/// classifiers, replay by projection, refine-until-resolved; verdicts
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
  /// The certificate document (`.hsc` s-expressions, spec §6) when
  /// v.k == holds, else empty.
  std::string certificate;
  /// On violation: every leaf's model value at the witness's end
  /// (leaf order) — the validated bad state, ready to bind.
  std::vector<std::int32_t> final_values;
  /// Coarse wall split, nanoseconds: abstract search, replay, and
  /// refinement — the leaf oracles (membership, certification) live
  /// inside refinement, so ns_refine is the teacher-cost column.
  std::int64_t ns_search = 0, ns_replay = 0, ns_refine = 0;
};

/// Algorithm 4.1 of the paper on model m. Round assertions and the
/// budget cap are enforced with hard aborts — a failure is a bug in the
/// learner or the teacher, not a condition to handle. The abstract
/// search cap is the exception: it is a resource limit, and hitting it
/// (an abstraction whose product outgrows it — many fine leaves, ghost
/// states) returns `verdict::kind::cap` for the caller to diagnose.
[[nodiscard]] run_result run(const model& m, const options& opt);

}  // namespace hsc::cegar
