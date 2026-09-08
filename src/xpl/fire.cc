/// \file fire.cc
/// \brief The term interpreter: filters test where they stand, updates
/// write simultaneously, havoc and `alt` fork, `abort` kills.
/// `interpret/algorithm.md` §2.

#include "hsc/xpl/interpret/fire.hh"

#include <algorithm>
#include <cstdint>
#include <string>

#include "hsc/xpl/interpret/eval.hh"

namespace hsc::xpl {

namespace {

struct branch {
  word s;
  std::vector<std::uint32_t> touched;  ///< written positions, dups allowed
};

/// Resolve an action's target to one frontier position, on the update
/// pre-state \p ev reads.
std::uint32_t resolve(const target& t, evaluator& ev) {
  if (!t.indexed) return t.cells.front();
  const std::int64_t i = ev.strict_int(t.index);
  if (i < 0 || i >= static_cast<std::int64_t>(t.cells.size())) {
    throw eval_error("target index " + std::to_string(i) +
                     " out of bounds (" + std::to_string(t.cells.size()) +
                     " cells)");
  }
  return t.cells[static_cast<std::size_t>(i)];
}

/// One update applied to one branch: reads on the pre-state, writes
/// simultaneous, havoc forks. Appends the outcomes to \p next.
void apply_update(const model& m, const clause& c, branch&& b,
                  std::vector<branch>& next) {
  evaluator ev(*m.ex, b.s);
  struct upd {
    std::uint32_t pos;
    value v;
  };
  std::vector<upd> writes;
  struct hav {
    std::uint32_t pos;
    value lo, hi;
  };
  std::vector<hav> havocs;
  for (const action& a : c) {
    const std::uint32_t pos = resolve(a.lhs, ev);
    if (a.k == action::kind::assign) {
      // fold steps checked int32 already; the cast is exact
      writes.push_back({pos, static_cast<value>(ev.strict_int(a.rhs))});
    } else {
      havocs.push_back({pos, a.lo, a.hi});
    }
  }
  // one update, one write per position: simultaneity has no tiebreak
  std::vector<std::uint32_t> targets;
  targets.reserve(writes.size() + havocs.size());
  for (const upd& w : writes) targets.push_back(w.pos);
  for (const hav& h : havocs) targets.push_back(h.pos);
  std::sort(targets.begin(), targets.end());
  if (std::adjacent_find(targets.begin(), targets.end()) != targets.end()) {
    throw eval_error("two writes to one position in one update");
  }
  for (const upd& w : writes) {
    b.s[w.pos] = w.v;
    b.touched.push_back(w.pos);
  }
  if (havocs.empty()) {
    next.push_back(std::move(b));
    return;
  }
  std::vector<branch> forks;
  forks.push_back(std::move(b));
  for (const hav& h : havocs) {
    std::vector<branch> wider;
    for (branch& f : forks) {
      for (value v = h.lo; v < h.hi; ++v) {
        branch y = f;
        y.s[h.pos] = v;
        y.touched.push_back(h.pos);
        wider.push_back(std::move(y));
      }
    }
    forks = std::move(wider);
  }
  for (branch& f : forks) next.push_back(std::move(f));
}

/// Interpret term \p ti over \p branches, in place.
void interp(const model& m, std::uint32_t ti, std::vector<branch>& branches) {
  if (branches.empty()) return;
  const term& t = m.pool[ti];
  switch (t.k) {
    case term::kind::filter: {
      std::vector<branch> kept;
      for (branch& b : branches) {
        evaluator ev(*m.ex, b.s);
        if (ev.decide(t.guard)) kept.push_back(std::move(b));
      }
      branches = std::move(kept);
      return;
    }
    case term::kind::update: {
      std::vector<branch> next;
      for (branch& b : branches) apply_update(m, t.acts, std::move(b), next);
      branches = std::move(next);
      return;
    }
    case term::kind::seq:
      for (const std::uint32_t kid : t.kids) {
        interp(m, kid, branches);
        if (branches.empty()) return;
      }
      return;
    case term::kind::alt: {
      std::vector<branch> all;
      for (const std::uint32_t kid : t.kids) {
        std::vector<branch> copy = branches;
        interp(m, kid, copy);
        for (branch& b : copy) all.push_back(std::move(b));
      }
      branches = std::move(all);
      return;
    }
    case term::kind::abort:
      branches.clear();
      return;
  }
}

}  // namespace

bool quick_enabled(const model& m, std::uint32_t e, state_view s) {
  evaluator ev(*m.ex, s);
  switch (ev.guard(m.events[e].quick)) {
    case lia::expr_factory::truth::yes:
      return true;
    case lia::expr_factory::truth::no:
      return false;
    default:
      throw interp_error(e, "quick test is ⊥: " + ev.cause(),
                         word(s.begin(), s.end()));
  }
}

void fire(const model& m, std::uint32_t e, state_view s,
          std::vector<successor>& out) {
  std::vector<branch> branches;
  branches.push_back({word(s.begin(), s.end()), {}});
  try {
    interp(m, m.events[e].root, branches);
  } catch (const eval_error& err) {
    throw interp_error(e, err.what(), word(s.begin(), s.end()));
  }
  for (branch& b : branches) {
    std::sort(b.touched.begin(), b.touched.end());
    b.touched.erase(std::unique(b.touched.begin(), b.touched.end()),
                    b.touched.end());
    successor sc{std::move(b.s), {}};
    for (const std::uint32_t p : b.touched) {
      if (sc.s[p] != s[p]) sc.changed.push_back(p);  // restored ≠ changed
    }
    out.push_back(std::move(sc));
  }
}

}  // namespace hsc::xpl
