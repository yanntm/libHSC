/// \file surface_families.cc
/// \brief Certified uniform families: the declared head-folded route, the
/// enumerated route, and the check mode that requires the same code.

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <set>

#include "surface_translator.hh"

namespace hsc::surface {

void translator::collect_markers(const datum& d, std::vector<fam_access>& out) {
  if (!d.is_list() || d.items().empty()) return;
  if (d.head() == "at@") {
    out.push_back({d.items()[1].text(), std::stoll(d.items()[2].text())});
    return;
  }
  for (const datum& k : d.items()) collect_markers(k, out);
}

/// Materialize instance \p i of a family datum: `(at@ a δ)` becomes the
/// cell atom of component (i+δ) mod n; everything else is untouched.
datum translator::instantiate(const datum& d, long long i, long long n) {
  if (!d.is_list() || d.items().empty()) return d;
  if (d.head() == "at@") {
    const array_decl& a = arrays_.at(d.items()[1].text());
    const long long c = ((i + std::stoll(d.items()[2].text())) % n + n) % n;
    return datum::atom(a.cells[static_cast<std::size_t>(c)], d.line());
  }
  std::vector<datum> kids;
  kids.reserve(d.items().size());
  for (const datum& k : d.items()) kids.push_back(instantiate(k, i, n));
  return datum::list(std::move(kids), d.line());
}

/// Instance \p i compiled at \p sort (frontier from \p lo). Deadness was
/// decided once on the representative — the family is uniform.
code translator::fam_instance_at(const fam_geometry& g, long long i, core::shape_code s,
                     std::size_t lo) {
  std::vector<datum> inst;
  inst.reserve(g.body.size());
  for (const datum& cl : g.body) inst.push_back(instantiate(cl, i, g.n));
  bool dead = false;
  return compile_event_at(*g.at, inst, s, lo, dead);
}

/// \brief The declared fold: the sum of instances
/// \p ids — each with every cell in [lo, lo+width) — as a head-folded
/// chain built by recursion over the sort tree, no site enumerated at
/// shared blocks.
///
/// An instance straddling this cut is compiled here, flat; the sides
/// recurse. Memoized on (sort, lo mod period): equal sorts at equal
/// alignment hold translation-conjugate instances, whose terms are equal
/// codes by currification (the wrap-around instance straddles the root
/// cut, whose sort no inner block shares). The result is code-for-code
/// the `sum_at` of the enumerated instances — check mode asserts it.
code translator::fold_family(const fam_geometry& g, core::shape_code s, std::size_t lo,
                 std::vector<long long>&& ids, fam_memo& memo) {
  if (ids.empty()) return core::op_table::id;
  const auto key =
      std::make_pair(s, static_cast<long long>(lo) % g.period);
  const auto hit = memo.find(key);
  if (hit != memo.end()) return hit->second;
  code result = core::op_table::id;
  if (mgr_.shapes().kind(s) != core::shape_kind::pair) {
    // whole instances inside one leaf: theory terms, and the theory owns
    // their sum (the leaf case of sum_at)
    core::support_algebra& algebra = mgr_.algebra(s);
    for (const long long i : ids) {
      const code t = fam_instance_at(g, i, s, lo);
      result =
          result == core::op_table::id ? t : algebra.term_sum(result, t);
    }
  } else {
    const core::shape_code h = mgr_.shapes().head(s);
    const core::shape_code t = mgr_.shapes().tail(s);
    const std::size_t mid = lo + mgr_.shapes().width(h);
    std::vector<long long> hs, ts;
    std::vector<code> parts;
    for (const long long i : ids) {
      if (g.max_pos[static_cast<std::size_t>(i)] < mid) {
        hs.push_back(i);
      } else if (g.min_pos[static_cast<std::size_t>(i)] >= mid) {
        ts.push_back(i);
      } else {
        parts.push_back(fam_instance_at(g, i, s, lo));
      }
    }
    const code hc = fold_family(g, h, lo, std::move(hs), memo);
    const code tc = fold_family(g, t, mid, std::move(ts), memo);
    if (hc != core::op_table::id) {
      parts.push_back(mgr_.operations().node(hc, core::op_table::id));
    }
    if (tc != core::op_table::id) {
      parts.push_back(mgr_.operations().node(core::op_table::id, tc));
    }
    result = mgr_.operations().sum(parts);
  }
  memo.emplace(key, result);
  return result;
}

/// `(family NAME N CLAUSE…)`: a certified uniform family, one term for
/// all N instances. Routes: `declared` builds the head-folded chain by
/// recursion (O(distinct blocks) term constructions); `unfold` compiles
/// every instance and `sum_at`s the list; `check` — the default — does
/// both and requires the same code, which canonicity makes an exact
/// gate. A layout that is not index-periodic falls back to unfold with
/// a note.
void translator::do_family(const datum& form) {
  if (top_ == core::none) fail(form, "family before shape");
  const std::string& name = sym(arg(form, 1, "family name"));
  const long long n = as_int(arg(form, 2, "family size"));
  if (n <= 0) fail(form, "family size must be positive");
  const std::span<const datum> body = std::span(form.items()).subspan(3);

  fam_geometry g{&form, body, n, 1, {}, {}};

  // The representative decides deadness once: the family is uniform.
  bool dead = false;
  {
    std::vector<datum> inst;
    for (const datum& cl : body) inst.push_back(instantiate(cl, 0, n));
    compile_event_at(form, inst, top_, 0, dead);
  }
  if (dead) {
    dead_event(form, name);
    return;
  }

  std::vector<fam_access> acc;
  for (const datum& cl : body) collect_markers(cl, acc);

  // C4 — index-periodic layout: every family array's cells sit at
  // pos(a_i) = pos(a_0) + i·period, one period for all. The certificate
  // upstream checked the indexing; the layout is this file's shape.
  bool foldable = !acc.empty();
  long long period = 0;
  std::map<std::string, std::vector<std::uint32_t>> pos;
  for (const fam_access& a : acc) {
    if (pos.contains(a.arr)) continue;
    const auto resolved = array(a.arr);
    if (!resolved || resolved->size() != static_cast<std::size_t>(n)) {
      fail(form, "family '" + name + "': array '" + a.arr +
                     "' does not have " + std::to_string(n) + " cells");
    }
    pos.emplace(a.arr, *resolved);
  }
  for (const auto& [arr, ps] : pos) {
    if (n == 1) continue;
    const long long step = static_cast<long long>(ps[1]) - ps[0];
    if (step <= 0) {
      foldable = false;
      break;
    }
    if (period == 0) period = step;
    if (step != period) {
      foldable = false;
      break;
    }
    for (long long i = 0; i < n; ++i) {
      if (ps[static_cast<std::size_t>(i)] !=
          ps[0] + static_cast<std::uint32_t>(i * step)) {
        foldable = false;
        break;
      }
    }
    if (!foldable) break;
  }
  // C5 — closed support: every leaf the body touches sits in a family
  // array. A shared scalar (a counter, a monitor) is index-invariant,
  // not index-periodic: the per-instance extents below would not cover
  // it and the fold would descend past its position. Such a family is
  // sound but not foldable — the enumerated route owns it.
  bool shared_leaf = false;
  if (foldable) {
    std::set<std::uint32_t> fam_cells;
    for (const auto& [arr, ps] : pos) fam_cells.insert(ps.begin(), ps.end());
    auto walk = [&](auto&& self, const datum& d) -> void {
      if (shared_leaf) return;
      if (d.is_atom()) {
        const auto p = position(d.text());
        if (p && !fam_cells.contains(*p)) shared_leaf = true;
        return;
      }
      for (const datum& k : d.items()) self(self, k);
    };
    for (const datum& cl : body) {
      const datum inst = instantiate(cl, 0, n);  // at@ markers -> cells
      walk(walk, inst);
    }
    if (shared_leaf) foldable = false;
  }
  if (period <= 0) period = 1;
  g.period = period;

  // Per-instance frontier extents, wrap included as it falls.
  if (!acc.empty()) {
    g.min_pos.assign(static_cast<std::size_t>(n), SIZE_MAX);
    g.max_pos.assign(static_cast<std::size_t>(n), 0);
    for (long long i = 0; i < n; ++i) {
      for (const fam_access& a : acc) {
        const long long c = ((i + a.delta) % n + n) % n;
        const std::size_t p = pos.at(a.arr)[static_cast<std::size_t>(c)];
        auto& mn = g.min_pos[static_cast<std::size_t>(i)];
        auto& mx = g.max_pos[static_cast<std::size_t>(i)];
        mn = std::min(mn, p);
        mx = std::max(mx, p);
      }
    }
  }

  code term;
  if (acc.empty()) {
    // no indexed access: all instances are one code already
    term = fam_instance_at(g, 0, top_, 0);
  } else {
    const bool want_unfold = !foldable || fmode_ != family_mode::declared;
    const bool want_fold = foldable && fmode_ != family_mode::unfold;
    code unfolded = core::none;
    code folded = core::none;
    if (want_unfold) {
      std::vector<code> ts;
      ts.reserve(static_cast<std::size_t>(n));
      for (long long i = 0; i < n; ++i) {
        ts.push_back(fam_instance_at(g, i, top_, 0));
      }
      unfolded = core::sum_at(mgr_, top_, ts);
    }
    if (want_fold) {
      std::vector<long long> all(static_cast<std::size_t>(n));
      std::iota(all.begin(), all.end(), 0);
      fam_memo memo;
      folded = fold_family(g, top_, 0, std::move(all), memo);
    }
    if (want_unfold && want_fold && folded != unfolded) {
      fail(form, "family '" + name +
                     "': declared fold and enumerated sum disagree — "
                     "fold gate violated, report this");
    }
    if (!foldable) {
      std::cerr << "note: family '" << name
                << (shared_leaf
                        ? "': touches leaves outside its arrays; enumerated"
                          " (line "
                        : "': layout is not index-periodic; enumerated"
                          " (line ")
                << form.line() << ")\n";
    }
    term = want_fold ? folded : unfolded;
  }

  define_event(form, name, term);
  if (term == core::op_table::id) return;
  events_.push_back(term);
  event_names_.push_back(name);
  guards_complete_ = false;  // a family's guards are not atoms: no (deadlock)
}

}  // namespace hsc::surface
