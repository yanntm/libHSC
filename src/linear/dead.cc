/// \file dead.cc
/// \brief The two deadness tests (`hsc/linear/dead.hh`).
#include "hsc/linear/dead.hh"

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"

namespace hsc::linear {

dead_report dead_transitions(core::manager& mgr, core::shape_code top, core::code set, core::code init,
                             std::span<const core::code> events, std::span<const core::code> guards,
                             std::span<const char> exact, bool with_step, std::size_t sharpen) {
  core::diagram_engine& d = mgr.diagrams();
  const std::size_t n = events.size();
  dead_report r;
  r.verdicts.assign(n, verdict::alive);
  // test 1: the guard selects nothing of the set
  std::vector<core::code> en(n, core::none);
  for (std::size_t i = 0; i < n; ++i) {
    if (i >= guards.size() || guards[i] == core::none) {
      r.verdicts[i] = verdict::untested;
      ++r.untested;
      continue;
    }
    en[i] = d.apply_local(guards[i], set);
    if (en[i] == core::none) {
      r.verdicts[i] = verdict::never_enabled;
      ++r.never_enabled;
    }
  }
  // test 2: the candidates are the alive events with an exact guard, not
  // enabled initially; the step is the sum of everything not proved dead
  const auto candidate = [&](std::size_t i) {
    return r.verdicts[i] == verdict::alive && (exact.empty() || exact[i]) &&
           d.apply_local(guards[i], init) == core::none;
  };
  const auto live_step = [&]() {
    std::vector<core::code> live;
    for (std::size_t i = 0; i < n; ++i)
      if (r.verdicts[i] == verdict::alive || r.verdicts[i] == verdict::untested) live.push_back(events[i]);
    return live.empty() ? core::none : core::sum_at(mgr, top, live);
  };
  const auto kill = [&](std::size_t i) {
    r.verdicts[i] = verdict::one_step;
    ++r.one_step;
  };
  for (bool changed = with_step && set != core::none; changed;) {
    changed = false;
    const core::code step = live_step();
    if (step == core::none) break;
    const core::code image = d.apply_local(step, set);
    ++r.rounds;
    std::vector<std::size_t> survivors;
    for (std::size_t i = 0; i < n; ++i) {
      if (!candidate(i)) continue;
      if (d.meet(en[i], image) == core::none) { kill(i); changed = true; }
      else survivors.push_back(i);
    }
    // the exact test for the survivors, while they are few: the image of
    // the markings that do not enable t, one per transition
    if (survivors.size() <= sharpen) {
      for (const std::size_t i : survivors) {
        const core::code from = d.minus(set, en[i]);
        const core::code img = from == core::none ? core::none : d.apply_local(step, from);
        if (d.meet(en[i], img) == core::none) { kill(i); changed = true; }
      }
    }
  }
  for (const verdict v : r.verdicts)
    if (v == verdict::alive) ++r.alive;
  return r;
}

}  // namespace hsc::linear
