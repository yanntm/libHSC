// M0 — the model kit: .cts round-trip; the paper's §6 worked example
// under the monolithic oracle, in both variants.

#include <doctest/doctest.h>

#include "hsc/cegar/gen.hh"
#include "hsc/cegar/model.hh"

using namespace hsc::cegar;

namespace {

// Paper §6: two clients and a server, "no grant while outstanding".
const char* kClientsServer = R"(
cts 1
leaf server states 3 letters 4
t 0 0 1   # F -g1-> B1
t 1 1 0   # B1 -r1-> F
t 0 2 2   # F -g2-> B2
t 2 3 0   # B2 -r2-> F
leaf cl1 states 2 letters 2
t 0 0 1
t 1 1 0
leaf cl2 states 2 letters 2
t 0 0 1
t 1 1 0
shape ( cl1 ( server cl2 ) )
event g1 server:0 cl1:0
event r1 server:1 cl1:1
event g2 server:2 cl2:0
event r2 server:3 cl2:1
monitor states 4
m 0 g1 1
m 1 r1 0
m 0 g2 2
m 2 r2 0
m 1 g1 3
m 1 g2 3
m 2 g1 3
m 2 g2 3
bad 3
)";

}  // namespace

TEST_CASE("cts parse and round-trip") {
  std::string err;
  auto m = parse_cts(kClientsServer, &err);
  REQUIRE(m);
  CHECK(m->leaves.size() == 3);
  CHECK(m->events.size() == 4);
  CHECK(m->mon.n_states == 4);
  // Unlisted monitor transitions self-loop.
  CHECK(m->mon.step(0, 1) == 0);  // r1 from quiet
  // Round-trip: print, re-parse, same structure.
  auto m2 = parse_cts(print_cts(*m), &err);
  REQUIRE(m2);
  CHECK(m2->leaves.size() == m->leaves.size());
  for (std::size_t i = 0; i < m->leaves.size(); ++i)
    CHECK(m2->leaves[i].serialize() == m->leaves[i].serialize());
  CHECK(m2->mon.delta == m->mon.delta);
}

TEST_CASE("parser rejects malformed input") {
  std::string err;
  CHECK(!parse_cts("leaf a states 1 letters 1\n", &err));  // no header
  CHECK(!parse_cts("cts 1\nleaf a states 2 letters 1\nt 0 0 1\nt 0 0 1\n"
                   "monitor states 1\n",
                   &err));  // duplicate transition
  CHECK(!parse_cts("cts 1\nleaf a states 1 letters 1\n"
                   "event e a:0 a:0\nmonitor states 1\n",
                   &err));  // leaf twice in support
}

TEST_CASE("mono: worked example holds; bugged variant violates and refires") {
  std::string err;
  auto m = parse_cts(kClientsServer, &err);
  REQUIRE(m);
  verdict v = mono(*m, 1'000'000);
  CHECK(v.k == verdict::kind::holds);
  CHECK(v.states_walked == 3);  // (m0,F,I,I), (m1,B1,C,I), (m2,B2,I,C)

  model bug = gen_clients_bug(2);
  verdict vb = mono(bug, 1'000'000);
  REQUIRE(vb.k == verdict::kind::violation);
  CHECK(vb.witness.size() == 2);
  CHECK(refire(bug, vb.witness));
}

TEST_CASE("generated families have the predicted verdicts") {
  CHECK(mono(gen_clients(3), 1'000'000).k == verdict::kind::holds);
  CHECK(mono(gen_ring(4), 1'000'000).k == verdict::kind::holds);
  CHECK(mono(gen_clients_bug(3), 1'000'000).k == verdict::kind::violation);
}
