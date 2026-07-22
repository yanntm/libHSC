#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "hsc/util/hash.hh"

using namespace hsc::util;

namespace {

/// A type opting in through a member hash(), the libHSC way.
struct node {
  std::uint32_t var;
  std::vector<std::uint32_t> sons;
  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = hash_value(var);
    hash_range(seed, sons.begin(), sons.end());
    return seed;
  }
};

/// Buckets \p values into \p buckets slots and returns the number of pairs
/// landing in a shared slot. A mixer worth the name keeps this near the
/// birthday-problem expectation rather than near the input's own structure.
std::size_t collisions(const std::vector<std::size_t>& hashes,
                       std::size_t buckets) {
  std::vector<std::size_t> counts(buckets, 0);
  for (auto h : hashes) ++counts[h & (buckets - 1)];
  std::size_t excess = 0;
  for (auto c : counts) excess += c > 1 ? c - 1 : 0;
  return excess;
}

}  // namespace

TEST_CASE("combine is not commutative") {
  std::size_t ab = 0, ba = 0;
  combine(ab, 17);
  combine(ab, 42);
  combine(ba, 42);
  combine(ba, 17);
  CHECK(ab != ba);
}

TEST_CASE("combine distinguishes a repeated element from a single one") {
  std::size_t once = 0, twice = 0;
  combine(once, 7);
  combine(twice, 7);
  combine(twice, 7);
  CHECK(once != twice);
}

TEST_CASE("hash_all separates composites of different arity") {
  CHECK(hash_all(1) != hash_all(1, 0));
  CHECK(hash_all(1, 0) != hash_all(1, 0, 0));
}

TEST_CASE("hash_range is order sensitive") {
  const std::array<std::uint32_t, 3> a{1, 2, 3};
  const std::array<std::uint32_t, 3> b{3, 2, 1};
  std::size_t ha = 0, hb = 0;
  hash_range(ha, a.begin(), a.end());
  hash_range(hb, b.begin(), b.end());
  CHECK(ha != hb);
}

TEST_CASE("hash_value dispatches to a member hash()") {
  const node n{3, {1, 2}};
  CHECK(hash_value(n) == n.hash());
  // permuting the sons must move the node: this is the legacy XOR bug
  const node m{3, {2, 1}};
  CHECK(hash_value(n) != hash_value(m));
}

TEST_CASE("hash_value handles std types and pointers") {
  CHECK(hash_value(std::string("abc")) == hash_value(std::string("abc")));
  CHECK(hash_value(std::string("abc")) != hash_value(std::string("abd")));
  int x = 0, y = 0;
  CHECK(hash_value(&x) != hash_value(&y));
}

TEST_CASE("mixers spread dense small integers") {
  // 4096 consecutive ids into 8192 buckets: the identity would collide zero
  // times but cluster; what we check is that mixing does not *create*
  // clustering. Expectation for a uniform hash is ~n^2/2m = 1024 excess.
  constexpr std::size_t n = 4096, buckets = 8192;
  std::vector<std::size_t> h64, h32;
  h64.reserve(n);
  h32.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    h64.push_back(mix64(i));
    h32.push_back(mix32(static_cast<std::uint32_t>(i)));
  }
  CHECK(collisions(h64, buckets) < 1400);
  CHECK(collisions(h32, buckets) < 1400);
}

TEST_CASE("combine spreads structured pairs") {
  // (i, j) over a small grid: the classic XOR combiner collides every
  // symmetric pair here, and a weak one clusters on the diagonal.
  std::vector<std::size_t> hs;
  std::unordered_set<std::size_t> distinct;
  for (std::uint32_t i = 0; i < 64; ++i) {
    for (std::uint32_t j = 0; j < 64; ++j) {
      const auto h = hash_all(i, j);
      hs.push_back(h);
      distinct.insert(h);
    }
  }
  CHECK(distinct.size() == hs.size());  // no exact duplicates at all
  // 4096 pairs into 8192 buckets: uniform expectation is ~n^2/2m = 1024.
  CHECK(collisions(hs, 8192) < 1400);
}
