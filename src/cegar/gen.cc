/// \file cegar/gen.cc — model families, deterministic in a seed.

#include "hsc/cegar/gen.hh"

#include <algorithm>
#include <random>

namespace hsc::cegar {

namespace {

/// A client: idle -0(grant)-> busy -1(release)-> idle.
lts client_lts() {
  lts c;
  c.n_states = 2;
  c.n_letters = 2;
  c.delta = {1, -1,   // idle: grant -> busy
             -1, 0};  // busy: release -> idle
  return c;
}

/// The server over 2k letters (grant_i = 2i, release_i = 2i+1):
/// free = 0, busy_i = i+1. `overlap_bug` leaves grants of clients 0 and
/// 1 enabled from each other's busy state.
lts server_lts(std::int32_t k, bool overlap_bug) {
  lts s;
  s.n_states = 1 + k;
  s.n_letters = 2 * k;
  s.delta.assign(static_cast<std::size_t>(s.n_states) * s.n_letters, -1);
  auto at = [&](std::int32_t q, std::int32_t a) -> std::int32_t& {
    return s.delta[static_cast<std::size_t>(q) * s.n_letters + a];
  };
  for (std::int32_t i = 0; i < k; ++i) {
    at(0, 2 * i) = i + 1;
    at(i + 1, 2 * i + 1) = 0;
  }
  if (overlap_bug && k >= 2) {
    at(1, 2 * 1) = 2;  // busy_0 grants client 1
    at(2, 2 * 0) = 1;  // busy_1 grants client 0
  }
  return s;
}

model clients_family(std::int32_t k, bool bug) {
  model m;
  m.leaves.push_back(server_lts(k, bug));
  m.leaf_names.push_back("server");
  for (std::int32_t i = 0; i < k; ++i) {
    m.leaves.push_back(client_lts());
    m.leaf_names.push_back("client" + std::to_string(i));
  }
  // Monitor: 0 = quiet, 1 = outstanding, 2 = bad (any grant while
  // outstanding).
  m.mon.n_states = 3;
  m.mon.bad = {false, false, true};
  for (std::int32_t i = 0; i < k; ++i) {
    event g, r;
    g.name = "g" + std::to_string(i);
    g.support = {{0, static_cast<letter>(2 * i)}, {i + 1, 0}};
    r.name = "r" + std::to_string(i);
    r.support = {{0, static_cast<letter>(2 * i + 1)}, {i + 1, 1}};
    m.events.push_back(std::move(g));
    m.events.push_back(std::move(r));
  }
  m.mon.n_events = static_cast<std::int32_t>(m.events.size());
  m.mon.delta.assign(
      static_cast<std::size_t>(m.mon.n_states) * m.mon.n_events, 0);
  auto at = [&m](std::int32_t s, std::int32_t e) -> std::int32_t& {
    return m.mon.delta[static_cast<std::size_t>(s) * m.mon.n_events + e];
  };
  for (std::int32_t e = 0; e < m.mon.n_events; ++e) {
    bool is_grant = (e % 2 == 0);
    at(0, e) = is_grant ? 1 : 0;
    at(1, e) = is_grant ? 2 : 0;
    at(2, e) = 2;
  }
  m.tree = shape::comb(static_cast<std::int32_t>(m.leaves.size()));
  return m;
}

}  // namespace

model gen_clients(std::int32_t k) { return clients_family(k, false); }
model gen_clients_bug(std::int32_t k) { return clients_family(k, true); }

model gen_ring(std::int32_t n) {
  // Station: 0 = idle, 1 = has token, 2 = critical.
  // Letters: 0 = receive token, 1 = pass token, 2 = enter, 3 = exit.
  model m;
  lts st;
  st.n_states = 3;
  st.n_letters = 4;
  st.delta = {1, -1, -1, -1,   // idle: receive
              -1, 0, 2, -1,    // token: pass | enter
              -1, -1, -1, 1};  // critical: exit (back to token)
  for (std::int32_t i = 0; i < n; ++i) {
    m.leaves.push_back(st);
    m.leaf_names.push_back("st" + std::to_string(i));
  }
  // Station 0 starts with the token: its own copy with initial = token.
  // States renumbered so 0 stays initial: 0 = token, 1 = idle, 2 = crit.
  lts st0;
  st0.n_states = 3;
  st0.n_letters = 4;
  st0.delta = {-1, 1, 2, -1,   // token: pass | enter
               0, -1, -1, -1,  // idle: receive
               -1, -1, -1, 0}; // critical: exit
  m.leaves[0] = st0;
  for (std::int32_t i = 0; i < n; ++i) {
    event pass, enter, exit;
    pass.name = "pass" + std::to_string(i);
    pass.support = {{i, 1}, {(i + 1) % n, 0}};
    std::sort(pass.support.begin(), pass.support.end());
    enter.name = "enter" + std::to_string(i);
    enter.support = {{i, 2}};
    exit.name = "exit" + std::to_string(i);
    exit.support = {{i, 3}};
    m.events.push_back(std::move(pass));
    m.events.push_back(std::move(enter));
    m.events.push_back(std::move(exit));
  }
  // Monitor: count stations in critical; two at once is bad.
  m.mon.n_states = 3;
  m.mon.bad = {false, false, true};
  m.mon.n_events = static_cast<std::int32_t>(m.events.size());
  m.mon.delta.assign(
      static_cast<std::size_t>(m.mon.n_states) * m.mon.n_events, 0);
  for (std::int32_t s = 0; s < 3; ++s)
    for (std::int32_t e = 0; e < m.mon.n_events; ++e) {
      std::int32_t d = s;
      if (m.events[e].name.rfind("enter", 0) == 0) d = s >= 2 ? 2 : s + 1;
      if (m.events[e].name.rfind("exit", 0) == 0 && s == 1) d = 0;
      m.mon.delta[static_cast<std::size_t>(s) * m.mon.n_events + e] = d;
    }
  m.tree = shape::comb(n);
  return m;
}

model gen_rand(std::int32_t l, std::int32_t q, std::int32_t s,
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
      ev.support.emplace_back(
          pick(0, l - 1),
          static_cast<letter>(0));
    // A letter may exceed that leaf's alphabet when the fallback fires
    // on a 1-letter leaf: clamp.
    for (auto& [lf, a] : ev.support)
      if (a >= m.leaves[lf].n_letters) a = 0;
    m.events.push_back(std::move(ev));
  }
  m.mon.n_states = pick(2, 4);
  m.mon.n_events = e;
  m.mon.delta.assign(
      static_cast<std::size_t>(m.mon.n_states) * e, 0);
  for (std::int32_t st = 0; st < m.mon.n_states; ++st)
    for (std::int32_t ev = 0; ev < e; ++ev)
      m.mon.delta[static_cast<std::size_t>(st) * e + ev] =
          pick(0, m.mon.n_states - 1);
  m.mon.bad.assign(m.mon.n_states, false);
  m.mon.bad[m.mon.n_states - 1] = true;
  m.tree = shape::comb(l);
  return m;
}

}  // namespace hsc::cegar
