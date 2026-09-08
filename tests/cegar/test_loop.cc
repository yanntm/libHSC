// The loop: verdict parity with the monolithic oracle (O1), budget
// (O3), the worked-example phenomenon, interning (O4 via stats), and
// the certificate emitter's shape.

#include <cstdint>
#include <doctest/doctest.h>

#include <sstream>

#include "fixtures.hh"
#include "hsc/cegar/loop.hh"
#include "rand_model.hh"

using namespace hsc::cegar;
using hsc::cegar::testing::build;

namespace {

void check_parity(const model& m, const options& opt = {}) {
  run_result r = run(m, opt);
  verdict vm = mono(m, 10'000'000);
  REQUIRE(vm.k != verdict::kind::cap);
  CHECK(r.v.k == vm.k);
  CHECK(r.cex_total <= r.budget);  // O3
  if (r.v.k == verdict::kind::violation) CHECK(refire(m, r.v.witness));
}

}  // namespace

TEST_CASE("the worked example: both entries driven exact, |Inv| = 3") {
  auto b = build(testing::kClients2, "(== mon 2)");
  run_result r = run(b.model, {});
  CHECK(r.v.k == verdict::kind::holds);
  CHECK(r.inv_size == 3);
  // The witness g1·g1 indicts server and client both (culprit sets are
  // search-order dependent); interning still
  // collapses the clients to one entry, and mon is the property leaf:
  // two entries total, both driven exact.
  CHECK(r.leaves_chaotic == 0);
  CHECK(r.leaves_exact == 2);
  CHECK(r.leaves_intermediate == 0);
  CHECK(r.budget == 4 + 3);  // server 3+1, client 2+1 (shared); mon none
}

TEST_CASE("violation: witness found, replayed, and valued") {
  auto b = build(testing::kClients2Bug, "(== mon 2)");
  run_result r = run(b.model, {});
  REQUIRE(r.v.k == verdict::kind::violation);
  CHECK(r.v.witness.size() == 2);
  CHECK(refire(b.model, r.v.witness));
  // The validated bad state: mon (last leaf) reached 2.
  REQUIRE(r.final_values.size() == 4);
  CHECK(r.final_values[3] == 2);
}

TEST_CASE("parity on a random grid, all policies") {
  for (std::uint64_t seed = 1; seed <= 40; ++seed) {
    model m = testing::rand_model(3, 4, 2, 5, 0.5, seed);
    if (mono(m, 200'000).k == verdict::kind::cap) continue;
    for (auto pick : {options::culprits::all, options::culprits::first,
                      options::culprits::cheapest}) {
      options opt;
      opt.pick = pick;
      check_parity(m, opt);
      opt.jump_exact = true;
      check_parity(m, opt);
    }
    options no_intern;
    no_intern.intern = false;
    check_parity(m, no_intern);
  }
}

TEST_CASE("regression: uncertified initial hypotheses were unsound") {
  // These seeds produced false 'holds' verdicts when the learner's
  // closed-but-uncertified initial table was published instead of chaos
  // (caught by the independent checker in the first sweep). Pinned
  // forever; rand_model reproduces the historical generator bit-exactly.
  check_parity(testing::rand_model(3, 4, 2, 5, 0.5, 72));
  for (std::uint64_t seed : {57, 67, 73, 96})
    check_parity(testing::rand_model(4, 5, 3, 8, 0.4, seed));
}

TEST_CASE("certificate document: the emitter's shape") {
  auto b = build(testing::kClients2, "(== mon 2)");
  run_result r = run(b.model, {});
  REQUIRE(r.v.k == verdict::kind::holds);
  const std::string& cert = r.certificate;
  CHECK(cert.find("(certificate (select (== mon 2)))") == 0);
  CHECK(cert.find("(classifier srv") != std::string::npos);
  CHECK(cert.find("(use cl2 cl1)") != std::string::npos);
  // mon is carried by inv values, not by a classifier.
  CHECK(cert.find("(classifier mon") == std::string::npos);
  int inv_forms = 0;
  for (std::size_t at = cert.find("(inv"); at != std::string::npos;
       at = cert.find("(inv", at + 1))
    ++inv_forms;
  CHECK(inv_forms == 3);
}
