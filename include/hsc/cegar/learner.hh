/// \file cegar/learner.hh
/// \brief L* with Rivest–Schapire counterexample handling, one instance
/// per (interned) leaf; membership is executing the leaf.
///
/// The observation table keeps prefix-closed access words S with pairwise
/// distinct rows and distinguishing suffixes E. `publish()` returns the
/// hypothesis as a canonical classifier: table DFA, dead-closed (soundness
/// is certification's burden, not construction's), canonicalized.
///
/// A counterexample is any word the *published* classifier misclassifies.
/// Because publication dead-closes, the word itself may agree with the raw
/// table; some prefix of it then disagrees (prefix-closed target), and the
/// handler locates the shortest such prefix and runs Rivest–Schapire
/// against the raw table. Every processed counterexample strictly grows
/// |S| (asserted), and |S| never exceeds the leaf's state count + 1 —
/// the budget that bounds the whole loop.

#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "hsc/cegar/classifier.hh"
#include "hsc/cegar/model.hh"

namespace hsc::cegar {

class learner {
 public:
  explicit learner(const lts& leaf);

  /// Process one misclassified word; strictly grows |S| or aborts.
  void add_counterexample(const word& w);

  /// The current hypothesis, canonical. Rebuilt lazily after growth.
  [[nodiscard]] const classifier& published();

  /// Counterexamples processed, all sources combined (the budget).
  [[nodiscard]] std::int64_t counterexamples() const { return n_cex_; }
  /// Live rows: the hypothesis index before canonicalization.
  [[nodiscard]] std::int64_t index() const {
    return static_cast<std::int64_t>(S_.size());
  }

 private:
  [[nodiscard]] bool member(const word& w);
  [[nodiscard]] std::vector<bool> row(const word& u);
  void close();
  [[nodiscard]] raw_table table();

  const lts* leaf_;
  std::vector<word> S_;
  std::vector<word> E_;
  std::map<word, bool> memo_;
  classifier published_;
  bool fresh_ = false;
  std::int64_t n_cex_ = 0;
};

}  // namespace hsc::cegar
