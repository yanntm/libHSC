/// \file test_split_equiv.cc
/// \brief The leaf-level `split_equiv`: partition a code by an expression's
/// value. The first step of the cross-level arithmetic path. Markers are
/// interned `lia` constants, or ⊥ where the expression is undefined.

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "hsc/leaves/int_set.hh"
#include "hsc/util/errors.hh"

using namespace hsc;

namespace {

/// The values a class holds, for terse assertions.
std::vector<std::int32_t> values(leaves::int_set_theory& t, core::code c) {
  const auto span = t.elements(c);
  return {span.begin(), span.end()};
}

}  // namespace

TEST_CASE("split_equiv by the coordinate gives one class per value") {
  leaves::int_set_theory t;
  const core::code s = t.interval(0, 5);  // {0,1,2,3,4}

  const auto classes = t.split_equiv(s, t.exprs().variable(0));
  REQUIRE(classes.size() == 5);
  for (const auto& [m, piece] : classes) {
    const std::int32_t v = t.exprs().value(m);
    CHECK(values(t, piece) == std::vector<std::int32_t>{v});
  }
}

TEST_CASE("split_equiv is keyed by the expression's realised values") {
  leaves::int_set_theory t;
  lia::expr_factory& ex = t.exprs();
  const core::code s = t.of(std::vector<std::int32_t>{0, 1, 2, 3, 4, 5});

  // x % 2 over {0..5} -> two classes, evens and odds.
  const auto classes =
      t.split_equiv(s, ex.mod(ex.variable(0), ex.constant(2)));
  REQUIRE(classes.size() == 2);
  for (const auto& [m, piece] : classes) {
    if (ex.value(m) == 0) {
      CHECK(values(t, piece) == std::vector<std::int32_t>{0, 2, 4});
    } else {
      CHECK(ex.value(m) == 1);
      CHECK(values(t, piece) == std::vector<std::int32_t>{1, 3, 5});
    }
  }
}

TEST_CASE("split_equiv puts undefined elements in the ⊥ class") {
  leaves::int_set_theory t;
  lia::expr_factory& ex = t.exprs();
  const core::code s = t.interval(0, 3);  // {0,1,2}

  // 4 % x: undefined at x = 0.
  const auto classes =
      t.split_equiv(s, ex.mod(ex.constant(4), ex.variable(0)));
  bool saw_undef = false;
  for (const auto& [m, piece] : classes) {
    if (m == lia::iundef) {
      saw_undef = true;
      CHECK(values(t, piece) == std::vector<std::int32_t>{0});
    }
  }
  CHECK(saw_undef);
}

TEST_CASE("split_equiv on the empty class is empty") {
  leaves::int_set_theory t;
  CHECK(t.split_equiv(core::none, t.exprs().variable(0)).empty());
}

TEST_CASE("split_equiv is loud on int32 overflow") {
  leaves::int_set_theory t;
  lia::expr_factory& ex = t.exprs();
  const core::code s = t.singleton(2'000'000'000);
  CHECK_THROWS_AS(
      (void)t.split_equiv(s, ex.mul(ex.constant(2), ex.variable(0))),
      hsc::overflow_error);
}
