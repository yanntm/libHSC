// Test support: a random cegar::model population, deterministic in a
// seed. Builds the core PODs directly — a free-form monitor and no
// property leaves, so every leaf is learnable — which is exactly the
// loop's contract and keeps the historical regression seeds meaningful.
#pragma once

#include <random>

#include "hsc/cegar/model.hh"

namespace hsc::cegar::testing {

inline model rand_model(std::int32_t l, std::int32_t q, std::int32_t s,
                        std::int32_t e, double d, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  auto pick = [&rng](std::int32_t lo, std::int32_t hi) {
    return static_cast<std::int32_t>(
        lo + rng() % static_cast<std::uint64_t>(hi - lo + 1));
  };
  model m;
  for (std::int32_t i = 0; i < l; ++i) {
    lts c;
    c.n_states = pick(2, q);
    c.n_letters = pick(1, s);
    c.delta.assign(static_cast<std::size_t>(c.n_states) * c.n_letters, -1);
    for (std::int32_t st = 0; st < c.n_states; ++st)
      for (std::int32_t a = 0; a < c.n_letters; ++a)
        if (rng() % 100 < 60)
          c.delta[static_cast<std::size_t>(st) * c.n_letters + a] =
              pick(0, c.n_states - 1);
    for (std::int32_t st = 0; st < c.n_states; ++st) c.value_of.push_back(st);
    m.leaves.push_back(std::move(c));
    m.leaf_names.push_back("l" + std::to_string(i));
  }
  for (std::int32_t i = 0; i < e; ++i) {
    event ev;
    ev.name = "e" + std::to_string(i);
    for (std::int32_t lf = 0; lf < l; ++lf)
      if (std::uniform_real_distribution<double>(0, 1)(rng) < d)
        ev.support.emplace_back(
            lf, static_cast<letter>(pick(0, m.leaves[lf].n_letters - 1)));
    if (ev.support.empty())
      ev.support.emplace_back(pick(0, l - 1), static_cast<letter>(0));
    // A letter may exceed that leaf's alphabet when the fallback fires
    // on a 1-letter leaf: clamp.
    for (auto& [lf, a] : ev.support)
      if (a >= m.leaves[lf].n_letters) a = 0;
    m.events.push_back(std::move(ev));
  }
  m.mon.n_states = pick(2, 4);
  m.mon.n_events = e;
  m.mon.delta.assign(static_cast<std::size_t>(m.mon.n_states) * e, 0);
  for (std::int32_t st = 0; st < m.mon.n_states; ++st)
    for (std::int32_t ev = 0; ev < e; ++ev)
      m.mon.delta[static_cast<std::size_t>(st) * e + ev] =
          pick(0, m.mon.n_states - 1);
  m.mon.bad.assign(m.mon.n_states, false);
  m.mon.bad[m.mon.n_states - 1] = true;
  m.monitor_values.assign(static_cast<std::size_t>(m.mon.n_states), {});
  return m;
}

}  // namespace hsc::cegar::testing
