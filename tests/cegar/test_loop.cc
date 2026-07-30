// M4+M5 — the loop: verdict parity with the monolithic oracle (O1),
// budget (O3), the §6 phenomenon, interning (O4 via stats), and the
// certificate obligations under mutation (O5, in-process mirror).

#include <doctest/doctest.h>

#include <sstream>

#include "hsc/cegar/gen.hh"
#include "hsc/cegar/loop.hh"

using namespace hsc::cegar;

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
  model m = gen_clients(2);
  run_result r = run(m, {});
  CHECK(r.v.k == verdict::kind::holds);
  CHECK(r.inv_size == 3);
  // The paper's §6 narrative (clients never touched) assumed the witness
  // g1·g2; BFS returns g1·g1 first, which the *client* also rejects — so
  // the shared client entry is a culprit too and both entries end exact.
  // Recorded as a finding in cegar_report.md: culprit sets are
  // search-order dependent. Interning still collapses all clients to one
  // entry, so the refinement count is K-independent.
  CHECK(r.leaves_chaotic == 0);
  CHECK(r.leaves_exact == 2);
  CHECK(r.leaves_intermediate == 0);
  CHECK(r.budget == 4 + 3);  // server 3+1, client 2+1 (shared)
}

TEST_CASE("violation: witness found and replayed") {
  model m = gen_clients_bug(2);
  run_result r = run(m, {});
  REQUIRE(r.v.k == verdict::kind::violation);
  CHECK(r.v.witness.size() == 2);
  CHECK(refire(m, r.v.witness));
}

TEST_CASE("parity across the families") {
  check_parity(gen_clients(2));
  check_parity(gen_clients(5));
  check_parity(gen_clients_bug(5));
  check_parity(gen_ring(3));
  check_parity(gen_ring(6));
}

TEST_CASE("parity on a random grid, all policies") {
  for (std::uint64_t seed = 1; seed <= 40; ++seed) {
    model m = gen_rand(3, 4, 2, 5, 0.5, seed);
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
  // (caught by hsc-certcheck in the first sweep). Pinned forever.
  check_parity(gen_rand(3, 4, 2, 5, 0.5, 72));
  for (std::uint64_t seed : {57, 67, 73, 96})
    check_parity(gen_rand(4, 5, 3, 8, 0.4, seed));
}

TEST_CASE("ring scales without touching the mono product") {
  model m = gen_ring(8);
  run_result r = run(m, {});
  CHECK(r.v.k == verdict::kind::holds);
  // Stations are interned: distinct entries are station 0 and the rest.
  CHECK(r.leaves_chaotic + r.leaves_intermediate + r.leaves_exact == 2);
}

TEST_CASE("certificate text: obligations hold and mutations break them") {
  model m = gen_clients(2);
  run_result r = run(m, {});
  REQUIRE(r.v.k == verdict::kind::holds);
  const std::string& cert = r.certificate;
  CHECK(cert.find("cert 1") == 0);
  CHECK(cert.find("classifier server") != std::string::npos);
  CHECK(cert.find("use client1 client0") != std::string::npos);
  // Inv has 3 lines of arity 4 (monitor + 3 leaves).
  int inv_lines = 0;
  std::istringstream is(cert);
  std::string line;
  while (std::getline(is, line))
    if (line.rfind("inv", 0) == 0) ++inv_lines;
  CHECK(inv_lines == 3);
  // The out-of-process mutation battery runs in the sweep scripts
  // against hsc-certcheck; here we assert the emitter's shape only.
}
