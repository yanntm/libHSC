// The trusted core (O5): every emitted certificate passes; mutated
// certificates fail — in-process, through the same entry the runner's
// (certcheck FILE) uses.

#include <doctest/doctest.h>

#include <sstream>

#include "fixtures.hh"
#include "hsc/cegar/loop.hh"
#include "hsc/surface/certcheck.hh"

using namespace hsc::cegar;
using namespace hsc::surface;
using hsc::cegar::testing::build;

namespace {

/// Check certificate text against the worked example's spec.
int check(const std::string& cert_text) {
  const std::vector<datum> forms =
      expand(parse(hsc::cegar::testing::kClients2), true, {});
  const spec s = spec::read(forms);
  hsc::lia::expr_factory ex;
  const expr_reader reader(ex, s);
  std::ostringstream sink;
  return certcheck(s, ex, reader, parse(cert_text), sink);
}

std::string emitted() {
  auto b = build(hsc::cegar::testing::kClients2, "(== mon 2)");
  run_result r = run(b.model, {});
  REQUIRE(r.v.k == verdict::kind::holds);
  return r.certificate;
}

/// One textual mutation; asserts it applied.
std::string mutate(std::string cert, const std::string& from,
                   const std::string& to) {
  const auto at = cert.find(from);
  REQUIRE(at != std::string::npos);
  cert.replace(at, from.size(), to);
  return cert;
}

}  // namespace

TEST_CASE("the emitted certificate passes the checker") {
  CHECK(check(emitted()) == 0);
}

TEST_CASE("mutations break the certificate") {
  const std::string cert = emitted();
  // Drop the first inv entry: G1 or G2 must fail.
  {
    const auto at = cert.find("(inv");
    const auto end = cert.find(')', cert.find("(inv"));
    std::string cut = cert;
    cut.erase(at, cert.find('\n', at) - at + 1);
    CHECK(check(cut) > 0);
  }
  // Weaken the property: the header no longer matches a proof of it.
  CHECK(check(mutate(cert, "(select (== mon 2))", "(select (== mon 1))")) >
        0);
  // Alias both clients to the wrong representative name.
  CHECK(check(mutate(cert, "(use cl2 cl1)", "(use cl2 srv)")) > 0);
}
