// The bridge (spec §5): letter induction, interning keys, the derived
// monitor, fragment refusals; the monolithic oracle on the worked
// example, both variants.

#include <cstdint>
#include <doctest/doctest.h>

#include "fixtures.hh"
#include "hsc/cegar/model.hh"

using namespace hsc::cegar;
using hsc::cegar::testing::build;

TEST_CASE("the worked example induces the paper's alphabets") {
  auto b = build(testing::kClients2, "(== mon 2)");
  const model& m = b.model;
  REQUIRE(m.leaves.size() == 4);
  REQUIRE(m.events.size() == 4);
  // srv: four distinct local actions; clients: grant/release; mon:
  // count-up (shared by g1,g2) and count-down (shared by r1,r2).
  CHECK(m.leaves[0].n_letters == 4);
  CHECK(m.leaves[1].n_letters == 2);
  CHECK(m.leaves[2].n_letters == 2);
  CHECK(m.leaves[3].n_letters == 2);
  // The two clients are byte-equal: one interning key.
  CHECK(m.leaves[1].serialize() == m.leaves[2].serialize());
  CHECK(m.leaves[0].serialize() != m.leaves[1].serialize());
  // mon is the one property leaf; its exact sub-product is the monitor.
  REQUIRE(m.prop_leaves == std::vector<std::int32_t>{3});
  CHECK(m.mon.n_states == 3);
  int bads = 0;
  for (bool x : m.mon.bad) bads += x;
  CHECK(bads == 1);
  // Values decode monitor states for the certificate.
  REQUIRE(m.monitor_values.size() == 3);
  CHECK(m.monitor_values[0] == std::vector<std::int32_t>{0});
}

TEST_CASE("two events with one local action share the letter") {
  auto b = build(R"(
(leaf a 0 2) (leaf x 0 2)
(shape (spine a x))
(init)
(event e1 (when (== a 0)) (do (:= a 1) (:= x 1)))
(event e2 (when (== a 0)) (do (:= a 1) (:= x 0)))
)",
                 "(== x 1)");
  CHECK(b.model.leaves[0].n_letters == 1);  // same graph on a
  CHECK(b.model.leaves[1].n_letters == 2);  // set-1 vs set-0 on x
}

TEST_CASE("fragment refusals are loud") {
  // Unbounded leaf.
  CHECK_THROWS(build(R"(
(leaf a) (leaf x 0 2) (shape (spine a x)) (init)
(event e (when (== a 0)) (do (:= a 1)))
)",
                     "(== x 1)"));
  // A guard atom crossing two leaves.
  CHECK_THROWS(build(R"(
(leaf a 0 2) (leaf x 0 2) (shape (spine a x)) (init)
(event e (when (< a x)) (do (:= a 1)))
)",
                     "(== x 1)"));
  // havoc.
  CHECK_THROWS(build(R"(
(leaf a 0 2) (leaf x 0 2) (shape (spine a x)) (init)
(event e (when (== a 0)) (do (havoc a 0 2)))
)",
                     "(== x 1)"));
  // An action reading another leaf.
  CHECK_THROWS(build(R"(
(leaf a 0 2) (leaf x 0 2) (shape (spine a x)) (init)
(event e (when (== a 0)) (do (:= a x)))
)",
                     "(== x 1)"));
  // An event driving a leaf out of its bound.
  CHECK_THROWS(build(R"(
(leaf a 0 2) (leaf x 0 2) (shape (spine a x)) (init)
(event e (when (== x 0)) (do (+= a 1) (:= x 1)))
)",
                     "(== x 1)"));
}

TEST_CASE("mono: worked example holds; bugged variant violates and refires") {
  auto b = build(testing::kClients2, "(== mon 2)");
  verdict v = mono(b.model, 1'000'000);
  CHECK(v.k == verdict::kind::holds);
  CHECK(v.states_walked == 3);  // (F,I,I,0), (B1,C,I,1), (B2,I,C,1)

  auto bug = build(testing::kClients2Bug, "(== mon 2)");
  verdict vb = mono(bug.model, 1'000'000);
  REQUIRE(vb.k == verdict::kind::violation);
  CHECK(vb.witness.size() == 2);
  CHECK(refire(bug.model, vb.witness));
}
