/// \file test_query.cc
/// \brief `x ⋈ y` across a cut (§7) against a brute-force oracle, for every
/// comparator. The first cross-level criterion resolving on the real engine.

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "hsc/core/manager.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/query.hh"

using namespace hsc;

namespace {

/// The full cube over `dims` domains, each `[0, size)`, as a right spine.
struct built {
  core::shape_code sort;
  core::code cube;
};

built cube(core::manager& mgr, leaves::int_set_theory& theory,
           core::shape_code leaf, const std::vector<int>& sizes) {
  core::diagram_engine& d = mgr.diagrams();
  // sort: spine of leaves; value: nested rectangles of full domains.
  std::vector<core::shape_code> sorts(sizes.size());
  core::shape_code sort = mgr.shapes().unit();
  for (std::size_t i = sizes.size(); i-- > 0;) {
    sort = mgr.shapes().pair(leaf, sort);
    sorts[i] = sort;
  }
  core::code val = d.one();
  for (std::size_t i = sizes.size(); i-- > 0;) {
    val = d.rectangle(sorts[i], theory.interval(0, sizes[i]), val);
  }
  return {sort, val};
}

/// The full cube over `sizes`, shaped as a balanced left-biased tree — the
/// shape Louvain-style decomposition produces, where a coordinate can sit at
/// any depth on either side of a cut.
built balanced_cube(core::manager& mgr, leaves::int_set_theory& theory,
                    core::shape_code leaf, const std::vector<int>& sizes,
                    std::size_t lo, std::size_t hi) {
  if (hi - lo == 1) return {leaf, theory.interval(0, sizes[lo])};
  const std::size_t mid = lo + (hi - lo + 1) / 2;
  const built h = balanced_cube(mgr, theory, leaf, sizes, lo, mid);
  const built t = balanced_cube(mgr, theory, leaf, sizes, mid, hi);
  const core::shape_code sort = mgr.shapes().pair(h.sort, t.sort);
  return {sort, mgr.diagrams().rectangle(sort, h.cube, t.cube)};
}

/// Brute-force count of tuples over `sizes` with tuple[xi] op tuple[yi].
double oracle(const std::vector<int>& sizes, std::size_t xi, cmp op,
              std::size_t yi) {
  double n = 0;
  std::vector<int> t(sizes.size(), 0);
  for (;;) {
    if (eval(op, t[xi], t[yi])) n += 1;
    std::size_t k = 0;
    for (; k < sizes.size(); ++k) {
      if (++t[k] < sizes[k]) break;
      t[k] = 0;
    }
    if (k == sizes.size()) break;
  }
  return n;
}

constexpr cmp ALL[] = {cmp::lt, cmp::le, cmp::eq, cmp::ne, cmp::ge, cmp::gt};

}  // namespace

TEST_CASE("every comparator at the top cut of a pair") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  const std::vector<int> sizes{3, 3};  // x in [0,3), y in [0,3)
  auto [sort, c] = cube(mgr, theory, leaf, sizes);

  for (const cmp op : ALL) {
    const core::code sel = select_compare(mgr, theory, sort, c, 0, op, 1);
    CHECK(mgr.diagrams().cardinal(sel) == oracle(sizes, 0, op, 1));
  }
}

TEST_CASE("every comparator with y a level deeper in the tail") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  // frontier: 0=x, 1=m (spectator), 2=y.  criterion x op y.
  const std::vector<int> sizes{3, 2, 3};
  auto [sort, c] = cube(mgr, theory, leaf, sizes);

  for (const cmp op : ALL) {
    const core::code sel = select_compare(mgr, theory, sort, c, 0, op, 2);
    CHECK(mgr.diagrams().cardinal(sel) == oracle(sizes, 0, op, 2));
  }
}

TEST_CASE("every comparator with x the later coordinate (head holds y)") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  // frontier: 0=y, 1=x.  criterion value(1) op value(0), i.e. x later.
  const std::vector<int> sizes{4, 4};
  auto [sort, c] = cube(mgr, theory, leaf, sizes);

  for (const cmp op : ALL) {
    const core::code sel = select_compare(mgr, theory, sort, c, 1, op, 0);
    CHECK(mgr.diagrams().cardinal(sel) == oracle(sizes, 1, op, 0));
  }
}

TEST_CASE("every comparator, every position pair, balanced shapes") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  // 4 leaves: pair(pair(l,l), pair(l,l)) — the head of the top cut is a pair,
  // so a crossing comparison must split a subdiagram, not a leaf. 5 leaves:
  // pair(pair(pair(l,l),l), pair(l,l)) — a depth-3 head.
  for (const std::vector<int> sizes :
       {std::vector<int>{3, 2, 3, 2}, std::vector<int>{2, 3, 2, 3, 2}}) {
    auto [sort, c] =
        balanced_cube(mgr, theory, leaf, sizes, 0, sizes.size());
    for (std::size_t x = 0; x < sizes.size(); ++x) {
      for (std::size_t y = 0; y < sizes.size(); ++y) {
        if (x == y) continue;
        for (const cmp op : ALL) {
          const core::code sel = select_compare(mgr, theory, sort, c, x, op, y);
          CHECK(mgr.diagrams().cardinal(sel) == oracle(sizes, x, op, y));
        }
      }
    }
  }
}

TEST_CASE("split_equiv on a diagram partitions it exactly") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  const std::vector<int> sizes{3, 2, 4, 2};
  auto [sort, c] = balanced_cube(mgr, theory, leaf, sizes, 0, sizes.size());
  for (std::size_t pos = 0; pos < sizes.size(); ++pos) {
    const auto classes = split_equiv(mgr, theory, sort, c, pos);
    REQUIRE(classes.size() == static_cast<std::size_t>(sizes[pos]));
    core::code all = core::none;
    double total = 0;
    std::int32_t prev = -1;
    for (const auto& [v, piece] : classes) {
      CHECK(v > prev);  // ascending, one class per value
      prev = v;
      total += mgr.diagrams().cardinal(piece);
      all = all == core::none ? piece : mgr.diagrams().join(all, piece);
      // each class is exactly the subset holding v at pos
      CHECK(piece ==
            select_in(mgr, theory, sort, c, pos, theory.singleton(v)));
    }
    CHECK(all == c);  // the classes cover, and cardinals confirm disjointness
    CHECK(total == mgr.diagrams().cardinal(c));
  }
}

TEST_CASE("x op x resolves on reflexivity") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  const std::vector<int> sizes{3, 2};
  auto [sort, c] = cube(mgr, theory, leaf, sizes);

  for (const cmp op : ALL) {
    const core::code sel = select_compare(mgr, theory, sort, c, 0, op, 0);
    CHECK(sel == (eval(op, 0, 0) ? c : core::none));
  }
}
