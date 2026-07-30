// M2+M3 — the teacher's certification walk and the L* learner driven to
// exactness with certification as the equivalence oracle.

#include <doctest/doctest.h>

#include "hsc/cegar/certify.hh"
#include "hsc/cegar/gen.hh"
#include "hsc/cegar/learner.hh"

using namespace hsc::cegar;

namespace {

// L(H) ⊆ L(leaf) gap finder, mirror of certification: a live word of h
// the leaf cannot fire (test-only equivalence oracle half).
std::optional<word> gap(const lts& l, const classifier& h) {
  const std::int32_t sink = l.n_states;
  const std::int32_t width = l.n_states + 1;
  std::vector<bool> seen(static_cast<std::size_t>(h.k) * width, false);
  std::vector<std::pair<std::int32_t, letter>> pred(seen.size(), {-1, 0});
  std::vector<std::int32_t> queue{0};
  seen[0] = true;
  for (std::size_t head = 0; head < queue.size(); ++head) {
    std::int32_t p = queue[head];
    std::int32_t c = p / width, q = p % width;
    for (std::int32_t a = 0; a < l.n_letters; ++a) {
      std::int32_t c2 = h.step(c, static_cast<letter>(a));
      if (!h.live(c2)) continue;
      std::int32_t q2 = q == sink ? sink : l.step(q, static_cast<letter>(a));
      if (q2 < 0) q2 = sink;
      std::int32_t p2 = c2 * width + q2;
      if (seen[p2]) continue;
      seen[p2] = true;
      pred[p2] = {p, static_cast<letter>(a)};
      queue.push_back(p2);
      if (q2 == sink) {
        word w;
        for (std::int32_t x = p2; pred[x].first >= 0; x = pred[x].first)
          w.push_back(pred[x].second);
        return word(w.rbegin(), w.rend());
      }
    }
  }
  return std::nullopt;
}

// Drive a learner to language equality with its leaf; return the final
// classifier. Asserts the budget |Q|+1 along the way.
classifier learn_exact(const lts& l) {
  learner lr(l);
  for (;;) {
    cert_result c = certify(l, lr.published());
    if (!c.certified) {
      lr.add_counterexample(c.counterexample);
      REQUIRE(lr.counterexamples() <= l.n_states + 1);
      continue;
    }
    auto g = gap(l, lr.published());
    if (!g) return lr.published();
    lr.add_counterexample(*g);
    REQUIRE(lr.counterexamples() <= l.n_states + 1);
  }
}

}  // namespace

TEST_CASE("chaos certifies for free; a too-strict table is refuted") {
  lts client;
  client.n_states = 2;
  client.n_letters = 2;
  client.delta = {1, -1, -1, 0};
  CHECK(certify(client, chaos(2)).certified);

  // Table rejecting everything after one letter: refuted by a trace.
  raw_table t;
  t.n_letters = 2;
  t.k = 2;
  t.act = {1, 1, 1, 1};
  t.live = {true, false};
  cert_result r = certify(client, canonicalize(t));
  REQUIRE(!r.certified);
  CHECK(client.fire(r.counterexample));            // it is a trace
  CHECK(!canonicalize(t).accepts(r.counterexample));  // the table rejects it
}

TEST_CASE("L* converges to the exact rung within budget") {
  // The §6 server: exact classifier has 4 classes (3 live + dead).
  lts server;
  server.n_states = 3;
  server.n_letters = 4;
  server.delta = {1, -1, 2, -1, -1, 0, -1, -1, -1, -1, -1, 0};
  classifier h = learn_exact(server);
  CHECK(h.k == 4);
  CHECK(h.dead >= 0);
  CHECK(h.accepts({0, 1, 2, 3}));   // g1 r1 g2 r2
  CHECK(!h.accepts({0, 2}));        // g1 g2

  // Client: 2 states -> 3 classes.
  lts client;
  client.n_states = 2;
  client.n_letters = 2;
  client.delta = {1, -1, -1, 0};
  CHECK(learn_exact(client).k == 3);
}

TEST_CASE("L* exactness on a random leaf population") {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    model m = gen_rand(1, 6, 3, 1, 1.0, seed);
    const lts& l = m.leaves[0];
    classifier h = learn_exact(l);
    // Language equality, both directions, by the two product walks.
    CHECK(certify(l, h).certified);
    CHECK(!gap(l, h));
    // Nerode bound.
    CHECK(h.k <= l.n_states + 1);
  }
}
