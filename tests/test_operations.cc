/// \file test_operations.cc
/// \brief Differential validation of the operation algebra.
///
/// Not unit tests: the oracle is the calculus's own equations, checked on
/// random shapes and random models. Diagrams are canonical, so "these two
/// computations agree" is an integer comparison, and any disagreement is a
/// real semantic difference rather than a formatting accident.
///
/// The load-bearing one is `saturate == naive fixpoint`. Saturation
/// reorganises the whole evaluation — pushing closures below a cut, chaining
/// crossing events — and the only thing that says it did not change the
/// answer is running the answer both ways.

#include <doctest/doctest.h>

#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"
#include "hsc/leaves/int_set.hh"

using namespace hsc;

namespace {

constexpr std::int32_t domain = 3;  // values 0..2: finite, so closures halt

/// The diagram of a single word, one value per leaf, left to right.
core::code point(core::manager& mgr, leaves::int_set_theory& theory,
                 core::shape_code sort, std::span<const std::int32_t> values,
                 std::size_t& next) {
  switch (mgr.shapes().kind(sort)) {
    case core::shape_kind::unit:
      return mgr.diagrams().one();
    case core::shape_kind::leaf:
      return theory.singleton(values[next++]);
    case core::shape_kind::pair: {
      const core::code head =
          point(mgr, theory, mgr.shapes().head(sort), values, next);
      const core::code tail =
          point(mgr, theory, mgr.shapes().tail(sort), values, next);
      return mgr.diagrams().rectangle(sort, head, tail);
    }
  }
  return core::none;
}

/// A random model: a shape, some events over it, and a starting set.
struct model {
  core::shape_code sort = 0;
  std::size_t leaves = 0;
  std::vector<core::code> events;
  core::code start = core::none;
};

core::shape_code random_shape(core::manager& mgr, core::shape_code leaf,
                              std::mt19937_64& rng, int n) {
  if (n == 1) return leaf;
  const int split = 1 + static_cast<int>(rng() % static_cast<unsigned>(n - 1));
  const core::shape_code head = random_shape(mgr, leaf, rng, split);
  const core::shape_code tail = random_shape(mgr, leaf, rng, n - split);
  return mgr.shapes().pair(head, tail);
}

model random_model(core::manager& mgr, leaves::int_set_theory& theory,
                   core::theory_index index, std::mt19937_64& rng) {
  const core::shape_code leaf = mgr.shapes().leaf(index);
  model m;
  // At least two leaves, so the top sort is a pair and there is a cut to
  // saturate across. The split is random, so heads are sometimes composite
  // (internalisation) and sometimes leaves.
  m.leaves = 2 + rng() % 4;
  m.sort = random_shape(mgr, leaf, rng, static_cast<int>(m.leaves));

  // A handful of starting words.
  std::vector<std::int32_t> values(m.leaves);
  for (int i = 0, points = 1 + static_cast<int>(rng() % 3); i < points; ++i) {
    for (auto& v : values) v = static_cast<std::int32_t>(rng() % domain);
    std::size_t next = 0;
    m.start = mgr.diagrams().join(
        m.start, point(mgr, theory, m.sort, values, next));
  }

  // Events: one local term per leaf, `id` where the event does not look.
  std::vector<core::code> by_leaf(m.leaves);
  for (int e = 0, count = 1 + static_cast<int>(rng() % 5); e < count; ++e) {
    for (std::size_t i = 0; i < m.leaves; ++i) {
      const auto choice = rng() % 4;
      const auto value = static_cast<std::int32_t>(rng() % domain);
      const core::code set = theory.singleton(value);
      by_leaf[i] = choice == 0   ? core::op_table::id
                   : choice == 1 ? theory.keep(set)
                   : choice == 2 ? theory.assign(core::none, value)
                                 : theory.assign(set, value);
    }
    m.events.push_back(
        core::product(mgr.operations(), mgr.shapes(), m.sort, by_leaf));
  }
  return m;
}

}  // namespace

TEST_CASE("saturation computes the same fixpoint as naive iteration") {
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    core::manager mgr;
    auto [index, theory] = mgr.import<leaves::int_set_theory>();
    std::mt19937_64 rng(seed);
    const model m = random_model(mgr, theory, index, rng);

    core::op_table& ops = mgr.operations();
    const core::code naive =
        mgr.diagrams().apply_local(ops.lfp(ops.sum(m.events)), m.start);
    const core::code saturated = mgr.diagrams().apply_local(
        core::saturate(mgr, m.sort, m.events), m.start);

    // Canonical, so this is the whole of the claim.
    CHECK(naive == saturated);
  }
}

/// Every word of \p c, as a value vector in frontier order.
void words_of(core::manager& mgr, leaves::int_set_theory& theory,
              core::shape_code sort, core::code c,
              std::vector<std::int32_t>& acc,
              std::vector<std::vector<std::int32_t>>& out) {
  switch (mgr.shapes().kind(sort)) {
    case core::shape_kind::unit:
      out.push_back(acc);
      return;
    case core::shape_kind::leaf:
      for (const std::int32_t v : theory.elements(c)) {
        acc.push_back(v);
        out.push_back(acc);
        acc.pop_back();
      }
      return;
    case core::shape_kind::pair: {
      const core::shape_code hs = mgr.shapes().head(sort);
      const core::shape_code ts = mgr.shapes().tail(sort);
      for (const core::arc& a : mgr.diagrams().arcs(c)) {
        std::vector<std::vector<std::int32_t>> heads;
        std::vector<std::int32_t> hacc;
        words_of(mgr, theory, hs, a.prime, hacc, heads);
        for (const auto& h : heads) {
          const std::size_t mark = acc.size();
          acc.insert(acc.end(), h.begin(), h.end());
          words_of(mgr, theory, ts, a.sub, acc, out);
          acc.resize(mark);
        }
      }
      return;
    }
  }
}

TEST_CASE("the inverse of a term is the converse relation on the reachable set") {
  for (std::uint64_t seed = 1; seed <= 150; ++seed) {
    core::manager mgr;
    auto [index, theory] = mgr.import<leaves::int_set_theory>();
    std::mt19937_64 rng(seed);
    const model m = random_model(mgr, theory, index, rng);
    core::diagram_engine& diagrams = mgr.diagrams();
    const core::code reach = diagrams.apply_local(
        core::saturate(mgr, m.sort, m.events), m.start);

    std::vector<std::vector<std::int32_t>> states;
    std::vector<std::int32_t> acc;
    words_of(mgr, theory, m.sort, reach, acc, states);

    core::inverter inv(mgr);
    for (const core::code e : m.events) {
      const core::code pe = inv(m.sort, e, reach);
      // Targets: the image of e, and a few random states of R.
      std::vector<core::code> targets{diagrams.apply_local(e, reach)};
      for (int i = 0; i < 2 && !states.empty(); ++i) {
        std::size_t next = 0;
        targets.push_back(point(mgr, theory, m.sort,
                                states[rng() % states.size()], next));
      }
      for (const core::code b : targets) {
        // Brute force: the reachable states with a successor in b.
        core::code brute = core::none;
        for (const auto& w : states) {
          std::size_t next = 0;
          const core::code pt = point(mgr, theory, m.sort, w, next);
          if (diagrams.meet(diagrams.apply_local(e, pt), b) != core::none) {
            brute = diagrams.join(brute, pt);
          }
        }
        const core::code got =
            diagrams.meet(diagrams.apply_local(pe, b), reach);
        CHECK(got == brute);
      }
    }
  }
}

TEST_CASE("composition of terms is composition of their actions") {
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    core::manager mgr;
    auto [index, theory] = mgr.import<leaves::int_set_theory>();
    std::mt19937_64 rng(seed);
    const model m = random_model(mgr, theory, index, rng);
    if (m.events.size() < 2) continue;

    core::diagram_engine& diagrams = mgr.diagrams();
    const core::code a = m.events[0];
    const core::code b = m.events[1];

    const core::code composed = diagrams.apply_local(
        mgr.operations().compose(a, b), m.start);
    const core::code by_hand =
        diagrams.apply_local(a, diagrams.apply_local(b, m.start));
    CHECK(composed == by_hand);
  }
}

TEST_CASE("a sum of terms is the union of their actions") {
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    core::manager mgr;
    auto [index, theory] = mgr.import<leaves::int_set_theory>();
    std::mt19937_64 rng(seed);
    const model m = random_model(mgr, theory, index, rng);
    if (m.events.size() < 2) continue;

    core::diagram_engine& diagrams = mgr.diagrams();
    const core::code summed = diagrams.apply_local(
        mgr.operations().sum(m.events[0], m.events[1]), m.start);
    const core::code by_hand =
        diagrams.join(diagrams.apply_local(m.events[0], m.start),
                      diagrams.apply_local(m.events[1], m.start));
    CHECK(summed == by_hand);
  }
}

TEST_CASE("id is free: applying it changes nothing") {
  for (std::uint64_t seed = 1; seed <= 50; ++seed) {
    core::manager mgr;
    auto [index, theory] = mgr.import<leaves::int_set_theory>();
    std::mt19937_64 rng(seed);
    const model m = random_model(mgr, theory, index, rng);
    CHECK(mgr.diagrams().apply_local(core::op_table::id, m.start) == m.start);
    CHECK(mgr.operations().node(core::op_table::id, core::op_table::id) ==
          core::op_table::id);
  }
}
