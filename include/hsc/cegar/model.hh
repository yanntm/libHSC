/// \file cegar/model.hh
/// \brief The verification input: finite LTS leaves synchronized by
/// product-shaped events under a safety monitor, on a binary shape.
///
/// A leaf's language is the set of letter words its partial deterministic
/// transition table fires to completion — nonempty (state 0 is initial),
/// prefix-closed. Events carry at most one letter per leaf (the separable
/// fragment); the monitor is a complete DFA over events, completeness by
/// the format convention that an unlisted transition is a self-loop.
/// Text form: the `.cts` format (see research_notes/cegar_spec.md §5).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace hsc::cegar {

using letter = std::uint16_t;
/// A word over one leaf's letters.
using word = std::vector<letter>;
/// A word over events, by event index.
using eword = std::vector<std::int32_t>;

/// A leaf: partial deterministic LTS, state 0 initial.
struct lts {
  std::int32_t n_states = 1;
  std::int32_t n_letters = 0;
  /// n_states x n_letters, -1 = undefined.
  std::vector<std::int32_t> delta;

  [[nodiscard]] std::int32_t step(std::int32_t q, letter a) const {
    return delta[static_cast<std::size_t>(q) * n_letters + a];
  }
  /// Membership: the reached state, or nullopt if some step is undefined.
  [[nodiscard]] std::optional<std::int32_t> fire(const word& w) const;
  /// Canonical bytes (header + delta table): the interning key.
  [[nodiscard]] std::string serialize() const;
};

/// An event: sorted per-leaf letters, at most one per leaf.
struct event {
  std::string name;
  std::vector<std::pair<std::int32_t, letter>> support;
  /// The letter this event gives leaf i, if supported.
  [[nodiscard]] std::optional<letter> letter_for(std::int32_t leaf) const;
};

/// Safety monitor: complete DFA over events, bad states absorbingly bad
/// only if the input says so — no completion beyond the self-loop rule.
struct monitor {
  std::int32_t n_states = 1;
  std::int32_t n_events = 0;
  /// n_states x n_events, complete.
  std::vector<std::int32_t> delta;
  std::vector<bool> bad;

  [[nodiscard]] std::int32_t step(std::int32_t m, std::int32_t e) const {
    return delta[static_cast<std::size_t>(m) * n_events + e];
  }
};

/// Binary shape over leaf indices; leaf nodes have leaf >= 0.
struct shape {
  struct node {
    std::int32_t leaf = -1;
    std::int32_t left = -1, right = -1;
  };
  std::vector<node> nodes;
  std::int32_t root = -1;
  /// The left comb over n leaves.
  static shape comb(std::int32_t n);
};

/// The bundle a `.cts` file describes.
struct model {
  std::vector<lts> leaves;
  std::vector<std::string> leaf_names;
  std::vector<event> events;
  monitor mon;
  shape tree;

  /// Projection of an event word onto leaf i.
  [[nodiscard]] word project(const eword& w, std::int32_t leaf) const;
};

/// Parse the `.cts` text form. On failure returns nullopt and sets *err.
[[nodiscard]] std::optional<model> parse_cts(const std::string& text,
                                             std::string* err);
[[nodiscard]] std::string print_cts(const model& m);

/// A verdict from a walk (monolithic or abstract).
struct verdict {
  enum class kind { holds, violation, cap };
  kind k = kind::holds;
  /// Nonempty only for violations.
  eword witness;
  /// States materialized by the walk that produced the verdict.
  std::int64_t states_walked = 0;
};

/// Monolithic oracle: BFS of the concrete product (m, q_1..q_n).
/// `cap` bounds materialized states; exceeding it yields kind::cap.
[[nodiscard]] verdict mono(const model& m, std::int64_t cap);

/// Concrete replay of an event word: true iff every projection fires
/// and the monitor ends in a bad state — i.e. the word is a violation.
[[nodiscard]] bool refire(const model& m, const eword& w);

}  // namespace hsc::cegar
