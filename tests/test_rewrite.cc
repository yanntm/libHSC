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

TEST_CASE("rewrite: hotbit trades an integer for one-hot bits") {
  // s cycles over {0..4}, compared and written by constants only; y is
  // a small bounded counter. 1 + 4*4 = 17 states, invariant under the
  // encoding, on both engines.
  const std::string body = R"(
(leaf s 0 8)
(leaf y 0 8)
(shape (spine s y))
(init)
(event step0 (when (== s 0)) (do (:= s 1)))
(event step1 (when (== s 1)) (do (:= s 2)))
(event step2 (when (== s 2)) (do (:= s 3)))
(event step3 (when (== s 3)) (do (:= s 4)))
(event wrap  (when (== s 4)) (do (:= s 0) (:= y 0)))
(event bump  (when (< y 3) (== s 1)) (do (+= y 1)))
(reach R saturate)
(expect R 17)
(xreach X)
(expect X 17)
)";
  const auto [rc0, out0] = run(body);
  CAPTURE(out0);
  CHECK(rc0 == 0);  // the un-encoded baseline
  const auto [rc, out] = run("(hotbit 3 16)" + body);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("one-hot encoded") != std::string::npos);
  CHECK(out.find("s: K=5") != std::string::npos);
  CHECK(out.find("ok R == 17") != std::string::npos);
  CHECK(out.find("ok X == 17") != std::string::npos);
}

TEST_CASE("rewrite: hotbit refuses a value read, loudly") {
  const std::string model = R"(
(hotbit 3 16)
(leaf s 0 8)
(leaf y 0 8)
(shape (spine s y))
(init)
(event a (when (== s 0)) (do (:= s 1)))
(event b (when (== s 1)) (do (:= s 2)))
(event c (when (== s 2)) (do (:= s 0)))
(event leak (when (== s 2)) (do (:= y s)))
(reach R)
(expect R 6)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("(identity)") != std::string::npos);
  CHECK(out.find("s refused: read as a value") != std::string::npos);
}

TEST_CASE("rewrite: simplify-arrays dissolves static-only groupings") {
  const std::string model = R"(
(simplify-arrays)
(leaf t0 0 3)
(leaf t1 0 3)
(leaf i 0 2)
(array t t0 t1)
(shape (spine t0 t1 i))
(init)
(event a (when (== (at t 0) 0)) (do (:= (at t 0) 1)))
(event b (when (== (at t 0) 1) (== i 0)) (do (:= (at t 1) 2) (:= i 1)))
(reach R)
(expect R 3)
(xreach X)
(expect X 3)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("1 array dissolved") != std::string::npos);
  CHECK(out.find("ok R == 3") != std::string::npos);
  CHECK(out.find("ok X == 3") != std::string::npos);
}

TEST_CASE("rewrite: simplify-arrays keeps dynamically accessed arrays") {
  const std::string model = R"(
(simplify-arrays)
(leaf t0 0 3)
(leaf t1 0 3)
(leaf i 0 2)
(array t t0 t1)
(shape (spine t0 t1 i))
(init)
(event a (when (== (at t i) 0)) (do (:= (at t i) 1) (:= i 1)))
(reach R)
(expect R 3)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("(identity)") != std::string::npos);
  CHECK(out.find("dynamic") != std::string::npos);
}

TEST_CASE("rewrite: reorder-force and flatten keep counts") {
  const std::string model = R"(
(reorder-force)
(leaf a 0 4)(leaf b 0 4)(leaf c 0 4)(leaf d 0 4)
(shape (balanced a b c d))
(init (a 3))
(event ab (when (> a 0)) (do (-= a 1) (+= b 1)))
(event bc (when (> b 0)) (do (-= b 1) (+= c 1)))
(event cd (when (> c 0)) (do (-= c 1) (+= d 1)))
(event da (when (> d 0)) (do (-= d 1) (+= a 1)))
(reach R)
(expect R 20)
(xreach X)
(expect X 20)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);  // whatever FORCE decides, counts hold on both engines
  const auto [rc2, out2] = run("(flatten)" + model.substr(16));
  CAPTURE(out2);
  CHECK(rc2 == 0);
  CHECK(out2.find("flat spine") != std::string::npos);
}

TEST_CASE("rewrite: print-spec emits the post-chain spec") {
  const std::string model = R"(
(simplify-constants)
(leaf c 0 5)
(leaf x 0 5)
(shape (spine c x))
(init (c 3))
(event a (when (== x 0) (> c 2)) (do (:= x c)))
(event b (when (== x 3)) (do (:= x 0)))
(print-spec)
(reach R)
(expect R 2)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("(leaf x") != std::string::npos);
  CHECK(out.find("(leaf c") == std::string::npos);   // elided
  CHECK(out.find("print-spec") == std::string::npos);  // not self-echoed
}
