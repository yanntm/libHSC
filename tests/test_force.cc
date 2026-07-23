/// \file test_force.cc
/// \brief FORCE (`hsc/order/force.hh`): permutation validity, span
/// reduction on scattered cliques, precedence orientation, determinism.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

#include "hsc/order/force.hh"

using namespace hsc::order;

namespace {

bool is_permutation_of(const std::vector<std::uint32_t>& v, std::size_t n) {
  std::vector<std::uint32_t> s = v;
  std::ranges::sort(s);
  std::vector<std::uint32_t> id(n);
  std::iota(id.begin(), id.end(), 0u);
  return s == id;
}

std::size_t rank_of(const std::vector<std::uint32_t>& order,
                    std::uint32_t var) {
  return static_cast<std::size_t>(
      std::ranges::find(order, var) - order.begin());
}

double span_cost(const std::vector<clique>& cs,
                 const std::vector<std::uint32_t>& order) {
  std::vector<std::size_t> rank(order.size());
  for (std::size_t i = 0; i < order.size(); ++i) rank[order[i]] = i;
  double cost = 0;
  for (const clique& c : cs) {
    std::size_t lo = order.size(), hi = 0;
    for (const std::uint32_t v : c.vars) {
      lo = std::min(lo, rank[v]);
      hi = std::max(hi, rank[v]);
    }
    cost += static_cast<double>(hi - lo);
  }
  return cost;
}

}  // namespace

TEST_CASE("no constraints: identity; result is always a permutation") {
  const auto id = force(5, {}, {});
  CHECK(id == std::vector<std::uint32_t>{0, 1, 2, 3, 4});
}

TEST_CASE("a scattered clique pulls contiguous") {
  // {0,3,5} scattered across six variables: identity span 5, optimal 2.
  std::vector<clique> cs{{{0, 3, 5}}};
  const auto order = force(6, cs, {});
  REQUIRE(is_permutation_of(order, 6));
  CHECK(span_cost(cs, order) == 2.0);
}

TEST_CASE("precedence orients a violated pair") {
  // identity puts 4 after 0; ask for the opposite.
  std::vector<precedence> ps{{4, 0, 1.0f}};
  const auto order = force(5, {}, ps);
  REQUIRE(is_permutation_of(order, 5));
  CHECK(rank_of(order, 4) < rank_of(order, 0));
}

TEST_CASE("deterministic: same input, same order") {
  std::vector<clique> cs{{{0, 7}}, {{7, 3}}, {{2, 5}}, {{5, 6}, 2.0f}};
  std::vector<precedence> ps{{6, 1}};
  CHECK(force(8, cs, ps) == force(8, cs, ps));
}
