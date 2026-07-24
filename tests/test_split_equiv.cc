/// \file test_split_equiv.cc
/// \brief The leaf-level `split_equiv`: partition a code by an expression's
/// value. The first step of the cross-level arithmetic path.

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

TEST_CASE("split_equiv by identity gives one class per value") {
  leaves::int_set_theory t;
  const core::code s = t.interval(0, 5);  // {0,1,2,3,4}

  const auto classes = t.split_equiv(s);  // coeff 1, offset 0
  REQUIRE(classes.size() == 5);
  for (std::int32_t v = 0; v < 5; ++v) {
    CHECK(classes[static_cast<std::size_t>(v)].first == v);
    CHECK(values(t, classes[static_cast<std::size_t>(v)].second) ==
          std::vector<std::int32_t>{v});
  }
}

TEST_CASE("split_equiv is keyed by the expression value, ascending") {
  leaves::int_set_theory t;
  const core::code s = t.of(std::vector<std::int32_t>{1, 3, 5});

  // 2*x + 1 over {1,3,5} -> keys {3,7,11}, each a singleton.
  const auto classes = t.split_equiv(s, 2, 1);
  REQUIRE(classes.size() == 3);
  CHECK(classes[0].first == 3);
  CHECK(classes[1].first == 7);
  CHECK(classes[2].first == 11);
  CHECK(values(t, classes[0].second) == std::vector<std::int32_t>{1});
  CHECK(values(t, classes[2].second) == std::vector<std::int32_t>{5});
}

TEST_CASE("split_equiv with a constant expression yields a single class") {
  leaves::int_set_theory t;
  const core::code s = t.interval(0, 4);  // {0,1,2,3}

  // coeff 0 -> every element maps to `offset`; one class holding the whole set.
  const auto classes = t.split_equiv(s, 0, 42);
  REQUIRE(classes.size() == 1);
  CHECK(classes[0].first == 42);
  CHECK(values(t, classes[0].second) == std::vector<std::int32_t>{0, 1, 2, 3});
}

TEST_CASE("split_equiv on the empty class is empty") {
  leaves::int_set_theory t;
  CHECK(t.split_equiv(core::none).empty());
}

TEST_CASE("split_equiv is loud on int32 overflow") {
  leaves::int_set_theory t;
  const core::code s = t.singleton(2'000'000'000);
  CHECK_THROWS_AS((void)t.split_equiv(s, 2, 0), hsc::overflow_error);
}
