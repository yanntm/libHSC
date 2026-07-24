/// \file test_rewrite.cc
/// \brief The rewrite chain: constant elision preserves counts on both
/// engines, the in-file directive runs the pass, identity is reported.

#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <utility>

#include "hsc/surface/expand.hh"
#include "hsc/surface/rewrite.hh"
#include "hsc/surface/sexpr.hh"
#include "hsc/surface/translate.hh"

namespace {

std::pair<int, std::string> run(const std::string& text) {
  std::ostringstream out;
  const auto forms = hsc::surface::expand(hsc::surface::parse(text),
                                          /*families=*/true, {});
  const int rc = hsc::surface::translate(forms, out);
  return {rc, out.str()};
}

// c is constant (init 3, only ever assigned 3); x copies it: {0, 3}.
const std::string kModel = R"(
(leaf c 0 5)
(leaf x 0 5)
(shape (spine c x))
(init (c 3))
(event a (when (== x 0) (> c 2)) (do (:= x c)))
(event b (when (== x 3)) (do (:= x 0) (:= c 3)))
(reach R saturate)
(expect R 2)
(xreach X)
(expect X 2)
)";

}  // namespace

TEST_CASE("rewrite: constant elision, counts invariant on both engines") {
  const auto [rc0, out0] = run(kModel);
  CAPTURE(out0);
  CHECK(rc0 == 0);  // the un-rewritten baseline

  auto forms = hsc::surface::expand(hsc::surface::parse(kModel), true, {});
  hsc::surface::rewrite_result res =
      hsc::surface::elide_constants(std::move(forms));
  CHECK(res.applied);
  CHECK(res.trace.find("c = 3") != std::string::npos);
  std::ostringstream out;
  CHECK(hsc::surface::translate(res.forms, out) == 0);
  CAPTURE(out.str());
  CHECK(out.str().find("ok R == 2") != std::string::npos);
  CHECK(out.str().find("ok X == 2") != std::string::npos);
}

TEST_CASE("rewrite: the (simplify-constants) directive, in file") {
  const auto [rc, out] = run("(simplify-constants)" + kModel);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("rewrite simplify-constants: 1 constant elided") !=
        std::string::npos);
  CHECK(out.find("ok R == 2") != std::string::npos);
  CHECK(out.find("ok X == 2") != std::string::npos);
}

TEST_CASE("rewrite: identity is reported, never silent") {
  const std::string model = R"(
(simplify-constants)
(leaf x 0 2)
(shape (spine x))
(init)
(event t (when (== x 0)) (do (:= x 1)))
(event u (when (== x 1)) (do (:= x 0)))
(reach R)
(expect R 2)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("rewrite simplify-constants (identity): no constants") !=
        std::string::npos);
}
