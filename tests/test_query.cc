/// \file test_query.cc
/// \brief `x < y` across a cut (§7) against a brute-force oracle. The first
/// cross-level criterion resolving on the real engine.

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

/// Brute-force count of tuples over `sizes` with tuple[xi] < tuple[yi].
double oracle(const std::vector<int>& sizes, std::size_t xi, std::size_t yi) {
  double n = 0;
  std::vector<int> t(sizes.size(), 0);
  for (;;) {
    if (t[xi] < t[yi]) n += 1;
    std::size_t k = 0;
    for (; k < sizes.size(); ++k) {
      if (++t[k] < sizes[k]) break;
      t[k] = 0;
    }
    if (k == sizes.size()) break;
  }
  return n;
}

}  // namespace

TEST_CASE("x < y at the top cut of a pair") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  const std::vector<int> sizes{3, 3};  // x in [0,3), y in [0,3)
  auto [sort, c] = cube(mgr, theory, leaf, sizes);

  const core::code sel = select_less_than(mgr, theory, sort, c, 0, 1);
  CHECK(mgr.diagrams().cardinal(sel) == oracle(sizes, 0, 1));  // {(0,1),(0,2),(1,2)} = 3
}

TEST_CASE("x < y with y a level deeper in the tail") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  // frontier: 0=x, 1=m (spectator), 2=y.  criterion x < y.
  const std::vector<int> sizes{3, 2, 3};
  auto [sort, c] = cube(mgr, theory, leaf, sizes);

  const core::code sel = select_less_than(mgr, theory, sort, c, 0, 2);
  CHECK(mgr.diagrams().cardinal(sel) == oracle(sizes, 0, 2));  // 3 * 2 = 6
}

TEST_CASE("x < y with x the later coordinate (head holds y)") {
  core::manager mgr;
  auto [idx, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(idx);

  // frontier: 0=y, 1=x.  criterion value(1) < value(0), i.e. x < y with x later.
  const std::vector<int> sizes{4, 4};
  auto [sort, c] = cube(mgr, theory, leaf, sizes);

  const core::code sel = select_less_than(mgr, theory, sort, c, 1, 0);
  CHECK(mgr.diagrams().cardinal(sel) == oracle(sizes, 1, 0));
}
