/// \file cegar/classifier.hh
/// \brief Canonical class tables: finite-index right congruences with at
/// most one dead class, absorbing, presented in shortlex order.
///
/// A classifier's language is the set of words whose class is live; it is
/// prefix-closed by dead-absorption. `canonicalize` maps any complete
/// live/dead table to the canonical classifier of its language, so
/// language equality is byte equality of `serialize()` — the property
/// interning and the certificate discipline rest on.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hsc/cegar/model.hh"

namespace hsc::cegar {

/// A complete class table before canonicalization: any number of dead
/// classes, transitions unrestricted. Class 0 is the class of ε.
struct raw_table {
  std::int32_t n_letters = 0;
  std::int32_t k = 1;
  /// k x n_letters.
  std::vector<std::int32_t> act;
  std::vector<bool> live;

  [[nodiscard]] std::int32_t classify(const word& w) const;
};

/// The canonical presentation: classes in shortlex order of their least
/// access words (reps[0] = ε), at most one dead class, absorbing.
struct classifier {
  std::int32_t n_letters = 0;
  std::int32_t k = 1;
  /// Index of the dead class, or -1 if the language is Σ*... of index k.
  std::int32_t dead = -1;
  /// k x n_letters.
  std::vector<std::int32_t> act;
  /// Shortlex-least access word per class, in shortlex order.
  std::vector<word> reps;

  [[nodiscard]] bool live(std::int32_t c) const { return c != dead; }
  [[nodiscard]] std::int32_t step(std::int32_t c, letter a) const {
    return act[static_cast<std::size_t>(c) * n_letters + a];
  }
  [[nodiscard]] std::int32_t classify(const word& w) const;
  [[nodiscard]] bool accepts(const word& w) const {
    return live(classify(w));
  }
  /// Canonical bytes (header + action + dead); reps are derived and
  /// excluded. Equality of classifiers is byte equality.
  [[nodiscard]] std::string serialize() const;
};

/// The one-class table of Σ*: the initial rung, certified for free.
[[nodiscard]] classifier chaos(std::int32_t n_letters);

/// Merge dead classes into one absorbing sink (dead-closure), minimize
/// (Moore), drop unreachable classes, rename in shortlex BFS order.
/// Idempotent; equal languages yield byte-equal results.
[[nodiscard]] classifier canonicalize(const raw_table& t);

}  // namespace hsc::cegar
