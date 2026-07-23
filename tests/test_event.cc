/// \file test_event.cc
/// \brief The §7 case bracket (`hsc/event.hh`) against brute-force oracles:
/// crossing filters, crossing updates, simultaneous writes, array
/// indirection with out-of-bounds abort — on spine and balanced shapes.

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <vector>

#include "hsc/core/manager.hh"
#include "hsc/event.hh"
#include "hsc/leaves/int_set.hh"

using namespace hsc;

namespace {

struct built {
  core::shape_code sort;
  core::code cube;
};

/// The full cube over `sizes`, each `[0, size)`, as a right spine.
built spine_cube(core::manager& mgr, leaves::int_set_theory& theory,
                 core::shape_code leaf, const std::vector<int>& sizes) {
  core::diagram_engine& d = mgr.diagrams();
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

/// The same cube as a balanced left-biased tree.
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

/// One word of `values` as a diagram of shape \p sort.
core::code word(core::manager& mgr, leaves::int_set_theory& theory,
                core::shape_code sort, const std::vector<int>& values,
                std::size_t& next) {
  switch (mgr.shapes().kind(sort)) {
    case core::shape_kind::unit:
      return mgr.diagrams().one();
    case core::shape_kind::leaf:
      return theory.singleton(values[next++]);
    case core::shape_kind::pair: {
      const core::code h = word(mgr, theory, mgr.shapes().head(sort), values,
                                next);
      const core::code t = word(mgr, theory, mgr.shapes().tail(sort), values,
                                next);
      return mgr.diagrams().rectangle(sort, h, t);
    }
  }
  return core::none;
}

/// Every tuple over `sizes` through \p f: `f` returns false to drop the
/// tuple (a refused guard or an abort), or edits it in place. The joined
/// results are the expected diagram.
core::code oracle(core::manager& mgr, leaves::int_set_theory& theory,
                  core::shape_code sort, const std::vector<int>& sizes,
                  const std::function<bool(std::vector<int>&)>& f) {
  core::code out = core::none;
  std::vector<int> t(sizes.size(), 0);
  for (;;) {
    std::vector<int> image = t;
    if (f(image)) {
      std::size_t next = 0;
      out = mgr.diagrams().join(out, word(mgr, theory, sort, image, next));
    }
    std::size_t k = 0;
    for (; k < sizes.size(); ++k) {
      if (++t[k] < sizes[k]) break;
      t[k] = 0;
    }
    if (k == sizes.size()) break;
  }
  return out;
}

struct rig {
  core::manager mgr;
  leaves::int_set_theory* theory;
  case_engine* cases;
  core::shape_code leaf;

  rig() {
    auto [idx, th] = mgr.import<leaves::int_set_theory>();
    theory = &th;
    cases = new case_engine(mgr, th);  // owned here, freed by process exit
    leaf = mgr.shapes().leaf(idx);
  }
  lia::expr_factory& ex() { return theory->exprs(); }
};

}  // namespace

TEST_CASE("crossing filter: when x0 < x2, spine and balanced") {
  rig r;
  const std::vector<int> sizes{3, 2, 4};
  for (const bool bal : {false, true}) {
    const built b = bal ? balanced_cube(r.mgr, *r.theory, r.leaf, sizes, 0,
                                        sizes.size())
                        : spine_cube(r.mgr, *r.theory, r.leaf, sizes);
    const lia::bexpr g = r.ex().compare(lia::bkind::lt, r.ex().variable(0),
                                        r.ex().variable(2));
    const core::code ev = r.cases->make_event(b.sort, g, {});
    const core::code got = r.mgr.diagrams().apply_local(ev, b.cube);
    const core::code want =
        oracle(r.mgr, *r.theory, b.sort, sizes,
               [](std::vector<int>& t) { return t[0] < t[2]; });
    CHECK(got == want);
  }
}

TEST_CASE("crossing update: x0 := x1 + x2, guarded") {
  rig r;
  const std::vector<int> sizes{3, 3, 3};
  for (const bool bal : {false, true}) {
    const built b = bal ? balanced_cube(r.mgr, *r.theory, r.leaf, sizes, 0,
                                        sizes.size())
                        : spine_cube(r.mgr, *r.theory, r.leaf, sizes);
    lia::expr_factory& ex = r.ex();
    // when x1 != 0 do x0 := x1 + x2
    const lia::bexpr g =
        ex.compare(lia::bkind::neq, ex.variable(1), ex.constant(0));
    const case_engine::assign as[] = {
        {ex.variable(0), ex.add(ex.variable(1), ex.variable(2))}};
    const core::code ev = r.cases->make_event(b.sort, g, as);
    const core::code got = r.mgr.diagrams().apply_local(ev, b.cube);
    const core::code want =
        oracle(r.mgr, *r.theory, b.sort, sizes, [](std::vector<int>& t) {
          if (t[1] == 0) return false;
          t[0] = t[1] + t[2];
          return true;
        });
    CHECK(got == want);
  }
}

TEST_CASE("simultaneous swap: x0 := x2, x2 := x0 in one clause") {
  rig r;
  const std::vector<int> sizes{3, 2, 3};
  const built b = spine_cube(r.mgr, *r.theory, r.leaf, sizes);
  lia::expr_factory& ex = r.ex();
  const case_engine::assign as[] = {{ex.variable(0), ex.variable(2)},
                                    {ex.variable(2), ex.variable(0)}};
  const core::code ev = r.cases->make_event(b.sort, lia::btrue, as);
  const core::code got = r.mgr.diagrams().apply_local(ev, b.cube);
  const core::code want =
      oracle(r.mgr, *r.theory, b.sort, sizes, [](std::vector<int>& t) {
        std::swap(t[0], t[2]);
        return true;
      });
  CHECK(got == want);
}

TEST_CASE("array write tab[i] := x with out-of-bounds abort") {
  rig r;
  // Frontier: tab_0 tab_1 tab_2 at 0..2, i at 3, x at 4. i ranges over
  // [0,5): indices 3 and 4 are out of bounds — those states abort.
  const std::vector<int> sizes{2, 2, 2, 5, 3};
  const std::uint32_t tab[] = {0, 1, 2};
  for (const bool bal : {false, true}) {
    const built b = bal ? balanced_cube(r.mgr, *r.theory, r.leaf, sizes, 0,
                                        sizes.size())
                        : spine_cube(r.mgr, *r.theory, r.leaf, sizes);
    lia::expr_factory& ex = r.ex();
    const case_engine::assign as[] = {
        {ex.array(tab, ex.variable(3)), ex.variable(4)}};
    const core::code ev = r.cases->make_event(b.sort, lia::btrue, as);
    const core::code got = r.mgr.diagrams().apply_local(ev, b.cube);
    const core::code want =
        oracle(r.mgr, *r.theory, b.sort, sizes, [](std::vector<int>& t) {
          if (t[3] >= 3) return false;  // abort contributes nothing
          t[t[3]] = t[4];
          return true;
        });
    CHECK(got == want);
  }
}

TEST_CASE("spread cells: tab interleaved with other leaves") {
  rig r;
  // tab's cells sit at positions 4, 0, 3 — spread and permuted through the
  // frontier; i at 1, x at 2. Placement is data, ordering-free.
  const std::vector<int> sizes{2, 4, 3, 2, 2};
  const std::uint32_t tab[] = {4, 0, 3};
  for (const bool bal : {false, true}) {
    const built b = bal ? balanced_cube(r.mgr, *r.theory, r.leaf, sizes, 0,
                                        sizes.size())
                        : spine_cube(r.mgr, *r.theory, r.leaf, sizes);
    lia::expr_factory& ex = r.ex();
    // when tab[i] != x do tab[i] := x  (i == 3 is out of bounds: abort)
    const lia::iexpr access = ex.array(tab, ex.variable(1));
    const lia::bexpr g =
        ex.compare(lia::bkind::neq, access, ex.variable(2));
    const case_engine::assign as[] = {{access, ex.variable(2)}};
    const core::code ev = r.cases->make_event(b.sort, g, as);
    const core::code got = r.mgr.diagrams().apply_local(ev, b.cube);
    const core::code want =
        oracle(r.mgr, *r.theory, b.sort, sizes, [&](std::vector<int>& t) {
          if (t[1] >= 3) return false;  // abort
          const std::size_t cell = tab[static_cast<std::size_t>(t[1])];
          if (t[cell] == t[2]) return false;  // guard refuses
          t[cell] = t[2];
          return true;
        });
    CHECK(got == want);
  }
}

TEST_CASE("indirection: when tab[tab[x]] == 0 filter (R4 gate)") {
  rig r;
  // tab_0..tab_2 at 0..2 over [0,3) — indices into itself — and x at 3.
  const std::vector<int> sizes{3, 3, 3, 3};
  const std::uint32_t tab[] = {0, 1, 2};
  for (const bool bal : {false, true}) {
    const built b = bal ? balanced_cube(r.mgr, *r.theory, r.leaf, sizes, 0,
                                        sizes.size())
                        : spine_cube(r.mgr, *r.theory, r.leaf, sizes);
    lia::expr_factory& ex = r.ex();
    const lia::iexpr inner = ex.array(tab, ex.variable(3));
    const lia::iexpr outer = ex.array(tab, inner);
    const lia::bexpr g = ex.compare(lia::bkind::eq, outer, ex.constant(0));
    const core::code ev = r.cases->make_event(b.sort, g, {});
    const core::code got = r.mgr.diagrams().apply_local(ev, b.cube);
    const core::code want =
        oracle(r.mgr, *r.theory, b.sort, sizes, [](std::vector<int>& t) {
          return t[t[t[3]]] == 0;  // both indices in range by construction
        });
    CHECK(got == want);
  }
}
