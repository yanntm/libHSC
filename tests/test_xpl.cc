/// \file test_xpl.cc
/// \brief The explicit engine through the surface: flat models where
/// `xreach` and the symbolic `reach` must agree, plus the error (TOP) and
/// Kleene disciplines, and the store's dedup.

#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <utility>

#include "hsc/surface/expand.hh"
#include "hsc/surface/sexpr.hh"
#include "hsc/surface/translate.hh"
#include "hsc/xpl/store.hh"

namespace {

/// Run a model text end to end; (failed expects, output).
std::pair<int, std::string> run(const std::string& text) {
  std::ostringstream out;
  const auto forms = hsc::surface::expand(hsc::surface::parse(text),
                                          /*families=*/true, {});
  const int rc = hsc::surface::translate(forms, out);
  return {rc, out.str()};
}

}  // namespace

TEST_CASE("xpl: store interns and deduplicates") {
  hsc::xpl::state_store st(3);
  const std::vector<hsc::xpl::value> a{1, 2, 3};
  const std::vector<hsc::xpl::value> b{1, 2, 4};
  const auto [ia, fa] = st.intern(a);
  CHECK(fa);
  const auto [ib, fb] = st.intern(b);
  CHECK(fb);
  CHECK(ia != ib);
  const auto [ia2, fa2] = st.intern(a);
  CHECK(!fa2);
  CHECK(ia2 == ia);
  CHECK(st.size() == 2);
  CHECK(st[ib][2] == 4);
  // survive a rehash: many distinct states, then look the first up again
  for (hsc::xpl::value v = 0; v < 3000; ++v) {
    (void)st.intern(std::vector<hsc::xpl::value>{v, v, v});
  }
  CHECK(st.intern(a) == std::pair{ia, false});
}

TEST_CASE("xpl: ring agrees with symbolic reach, from init and from a word") {
  const std::string ring = R"(
(leaf a 0 4)(leaf b 0 4)(leaf c 0 4)(leaf d 0 4)
(shape (spine a b c d))
(init (a 3))
(event ab (when (> a 0)) (do (-= a 1) (+= b 1)))
(event bc (when (> b 0)) (do (-= b 1) (+= c 1)))
(event cd (when (> c 0)) (do (-= c 1) (+= d 1)))
(event da (when (> d 0)) (do (-= d 1) (+= a 1)))
(reach R saturate)
(expect R 20)
(xreach X)
(expect X 20)
(count X)
(word w (a 1) (b 2))
(xreach Y from w)
(expect Y 20)
)";
  const auto [rc, out] = run(ring);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("X xreach 20 states") != std::string::npos);
}

TEST_CASE("xpl: flat binary counter, maintenance under many events") {
  constexpr int bits = 8;
  std::ostringstream m;
  for (int i = 0; i < bits; ++i) m << "(leaf b" << i << " 0 2)\n";
  m << "(shape (spine";
  for (int i = 0; i < bits; ++i) m << " b" << i;
  m << "))\n(init)\n";
  for (int i = 0; i < bits; ++i) {
    m << "(event inc" << i << " (when (== b" << i << " 0)";
    for (int j = 0; j < i; ++j) m << " (== b" << j << " 1)";
    m << ") (do (:= b" << i << " 1)";
    for (int j = 0; j < i; ++j) m << " (:= b" << j << " 0)";
    m << "))\n";
  }
  m << "(reach R saturate)\n(expect R 256)\n"
       "(xreach X)\n(expect X 256)\n";
  const auto [rc, out] = run(m.str());
  CAPTURE(out);
  CHECK(rc == 0);
}

TEST_CASE("xpl: havoc forks, sequential clauses compose") {
  const std::string model = R"(
(leaf x 0 5)
(shape (spine x))
(init)
(event fan (when (== x 0)) (do (havoc x 0 5)))
(event hop (when (== x 1)) (do (:= x 2)) (do (+= x 1)))
(reach R saturate)
(expect R 5)
(xreach X)
(expect X 5)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);
}

TEST_CASE("xpl: division by zero at a decision is TOP, loudly") {
  const std::string model = R"(
(leaf x 0 10)
(shape (spine x))
(init)
(event bad (when (== x 0)) (do (:= x (/ 1 x))))
(xreach X)
(expect X 1)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc >= 1);  // the TOP itself, and the expect against it
  CHECK(out.find("TOP") != std::string::npos);
  CHECK(out.find("division by zero") != std::string::npos);
  CHECK(out.find("(x 0)") != std::string::npos);  // the witness word
}

TEST_CASE("xpl: false absorbs bottom in a guard (Kleene)") {
  // at x == 0 the divisor guard is (false ∧ ⊥): disabled, not an error
  const std::string model = R"(
(leaf x 0 4)
(shape (spine x))
(init (x 2))
(event dec (when (> x 0)) (do (-= x 1)))
(event jump (when (> x 0) (>= (/ 4 x) 1)) (do (:= x 3)))
(xreach X)
(expect X 4)
)";
  const auto [rc, out] = run(model);
  CAPTURE(out);
  CHECK(rc == 0);
  CHECK(out.find("TOP") == std::string::npos);
}
