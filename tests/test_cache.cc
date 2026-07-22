#include <doctest/doctest.h>

#include <cstdint>

#include "hsc/mem/cache.hh"
#include "hsc/util/hash.hh"

using namespace hsc;

namespace {

// --- a memoized recursion: the cache re-enters itself ---------------------

struct fib_context;

struct fib_op {
  std::uint64_t n;
  friend bool operator==(const fib_op&, const fib_op&) = default;
  [[nodiscard]] std::size_t hash() const { return util::hash_value(n); }
  std::uint64_t operator()(fib_context& cxt) const;
};

struct fib_context {
  mem::cache<fib_context, fib_op>* memo = nullptr;
  std::uint64_t evaluations = 0;
};

std::uint64_t fib_op::operator()(fib_context& cxt) const {
  ++cxt.evaluations;
  if (n < 2) return n;
  return (*cxt.memo)(fib_op{n - 1}) + (*cxt.memo)(fib_op{n - 2});
}

// --- a flat operation, for exercising the replacement policy --------------

struct flat_context {
  std::uint64_t evaluations = 0;
};

struct flat_op {
  std::uint64_t n;
  friend bool operator==(const flat_op&, const flat_op&) = default;
  [[nodiscard]] std::size_t hash() const { return util::hash_value(n); }
  std::uint64_t operator()(flat_context& cxt) const {
    ++cxt.evaluations;
    return n * 2;
  }
};

/// An operation that declines to be cached, at compile time.
struct odd_only_op {
  std::uint64_t n;
  friend bool operator==(const odd_only_op&, const odd_only_op&) = default;
  [[nodiscard]] std::size_t hash() const { return util::hash_value(n); }
  [[nodiscard]] bool cacheable() const { return n % 2 == 1; }
  std::uint64_t operator()(flat_context& cxt) const {
    ++cxt.evaluations;
    return n;
  }
};

}  // namespace

TEST_CASE("a memoized recursion evaluates each subproblem once") {
  fib_context cxt;
  mem::cache<fib_context, fib_op> memo(cxt, 128);
  cxt.memo = &memo;

  CHECK(memo(fib_op{60}) == 1548008755920ull);
  // n = 0..60, each evaluated exactly once despite the exponential call tree
  CHECK(cxt.evaluations == 61);
  CHECK(memo.stats().misses == 61);
  CHECK(memo.size() == 61);
  CHECK(memo.stats().hits > 0);
  CHECK(memo.stats().evicted == 0);

  // asking again is answered entirely from the memo
  const auto before = cxt.evaluations;
  CHECK(memo(fib_op{60}) == 1548008755920ull);
  CHECK(cxt.evaluations == before);
}

TEST_CASE("eviction is least-recently-used") {
  flat_context cxt;
  mem::cache<flat_context, flat_op> memo(cxt, 4);

  for (std::uint64_t i = 1; i <= 4; ++i) CHECK(memo(flat_op{i}) == i * 2);
  CHECK(cxt.evaluations == 4);
  CHECK(memo.size() == 4);

  CHECK(memo(flat_op{1}) == 2);       // hit, and 1 becomes most recent
  CHECK(cxt.evaluations == 4);        // so 2 is now the least recent
  CHECK(memo(flat_op{5}) == 10);      // miss at capacity: evicts 2
  CHECK(memo.stats().evicted == 1);
  CHECK(memo.size() == 4);

  const auto before = cxt.evaluations;
  CHECK(memo(flat_op{1}) == 2);       // kept: it was touched
  CHECK(cxt.evaluations == before);
  CHECK(memo(flat_op{2}) == 4);       // gone: it was the oldest
  CHECK(cxt.evaluations == before + 1);
}

TEST_CASE("eviction batches are a knob") {
  flat_context cxt;
  mem::cache<flat_context, flat_op> memo(cxt, 8, /*evict_batch=*/4);

  for (std::uint64_t i = 1; i <= 8; ++i) memo(flat_op{i});
  CHECK(memo.size() == 8);
  memo(flat_op{9});  // one round drops four
  CHECK(memo.stats().evicted == 4);
  CHECK(memo.stats().evictions == 1);
  CHECK(memo.size() == 5);
}

TEST_CASE("capacity is never exceeded under churn") {
  flat_context cxt;
  mem::cache<flat_context, flat_op> memo(cxt, 16);
  for (std::uint64_t i = 0; i < 10000; ++i) {
    CHECK(memo(flat_op{i}) == i * 2);
    CHECK(memo.size() <= 16);
  }
  CHECK(cxt.evaluations == 10000);  // every operation was new
  CHECK(memo.stats().evicted == 10000 - 16);
}

TEST_CASE("an operation can decline to be cached") {
  flat_context cxt;
  mem::cache<flat_context, odd_only_op> memo(cxt, 64);
  for (int round = 0; round < 3; ++round) {
    for (std::uint64_t i = 0; i < 10; ++i) memo(odd_only_op{i});
  }
  CHECK(memo.stats().filtered == 15);  // the five even ops, three times
  CHECK(memo.size() == 5);             // only the odd ones were stored
  CHECK(cxt.evaluations == 5 + 15);    // odds once, evens every time
}

TEST_CASE("a runtime filter is the policy hook") {
  flat_context cxt;
  mem::cache<flat_context, flat_op> memo(cxt, 64);
  memo.set_filter([](const flat_op& op) { return op.n < 3; });

  for (int round = 0; round < 4; ++round) {
    for (std::uint64_t i = 0; i < 5; ++i) memo(flat_op{i});
  }
  CHECK(memo.size() == 3);
  CHECK(memo.stats().filtered == 8);  // n in {3,4}, four rounds
  CHECK(cxt.evaluations == 3 + 8);
}

TEST_CASE("clear drops everything and the cache stays usable") {
  flat_context cxt;
  mem::cache<flat_context, flat_op> memo(cxt, 8);
  for (std::uint64_t i = 0; i < 8; ++i) memo(flat_op{i});
  memo.clear();
  CHECK(memo.size() == 0);

  const auto before = cxt.evaluations;
  for (std::uint64_t i = 0; i < 8; ++i) CHECK(memo(flat_op{i}) == i * 2);
  CHECK(cxt.evaluations == before + 8);  // all recomputed
  CHECK(memo.size() == 8);
}
