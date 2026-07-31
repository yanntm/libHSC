/// \file cegar/model.hh
/// \brief The verification input: finite LTS leaves synchronized by
/// product-shaped events under a safety monitor.
///
/// A leaf's language is the set of letter words its partial deterministic
/// transition table fires to completion — nonempty (state 0 is initial),
/// prefix-closed. Events carry at most one letter per leaf (the separable
/// fragment). The monitor is the exact sub-product of the property
/// leaves, derived by the surface bridge (`hsc/surface/cegar_build.hh`)
/// from a parsed `.hsc` spec and the property atoms: complete over
/// events except where a property leaf blocks (`step` returns -1).
/// This package is parser-free; models arrive already built.

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
  /// State -> the leaf's value in the source model (witness assembly;
  /// not part of the interning key — sharing is language-level).
  std::vector<std::int32_t> value_of;

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

/// Safety monitor: the property leaves' exact sub-product over events.
/// `step` returns -1 where a property leaf blocks the event.
struct monitor {
  std::int32_t n_states = 1;
  std::int32_t n_events = 0;
  /// n_states x n_events; -1 = blocked.
  std::vector<std::int32_t> delta;
  std::vector<bool> bad;

  [[nodiscard]] std::int32_t step(std::int32_t m, std::int32_t e) const {
    return delta[static_cast<std::size_t>(m) * n_events + e];
  }
};

/// The bundle the surface bridge builds from a `.hsc` spec.
struct model {
  std::vector<lts> leaves;
  std::vector<std::string> leaf_names;
  std::vector<event> events;
  monitor mon;
  /// Leaves the property atoms name, sorted. Tracked exactly by the
  /// monitor: no classifier coordinate, never culprits, never interned,
  /// no budget (spec §5.6).
  std::vector<std::int32_t> prop_leaves;
  /// Per monitor state, the property-leaf values (prop_leaves order).
  std::vector<std::vector<std::int32_t>> monitor_values;
  /// The property atoms, rendered — echoed by the certificate header.
  std::string property_text;

  /// Projection of an event word onto leaf i.
  [[nodiscard]] word project(const eword& w, std::int32_t leaf) const;
  [[nodiscard]] bool is_prop(std::int32_t leaf) const;
};

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

/// Concrete replay of an event word: true iff every projection fires,
/// no monitor step blocks, and the monitor ends bad — a violation.
[[nodiscard]] bool refire(const model& m, const eword& w);

}  // namespace hsc::cegar
