/// \file dead.cc
/// \brief The two deadness tests (`hsc/linear/dead.hh`).
#include "hsc/linear/dead.hh"

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/util/errors.hh"

namespace hsc::linear {

dead_report dead_transitions(core::manager& mgr, core::shape_code, core::code set, core::code init,
                             std::span<const core::code> events, std::span<const core::code> guards,
                             std::span<const char> exact, const entry_fn& entries) {
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
  // test 2: no live event leads from the set outside the slice into it, the
  // slice not initial; the live events are those not proved dead (the
  // untested included: they may fire)
  if (entries && set != core::none) {
    std::vector<char> live(n, 1);
    std::vector<char> candidate(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
      live[i] = r.verdicts[i] != verdict::never_enabled;
      candidate[i] = r.verdicts[i] == verdict::alive && (exact.empty() || exact[i]) &&
                     d.apply_local(guards[i], init) == core::none;
    }
    try {
      for (bool changed = true; changed;) {
        changed = false;
        ++r.rounds;
        for (std::size_t i = 0; i < n; ++i) {
          if (!candidate[i]) continue;
          if (entries(i, en[i], live) != core::none) continue;
          r.verdicts[i] = verdict::one_step;
          ++r.one_step;
          candidate[i] = 0;
          live[i] = 0;
          changed = true;
        }
      }
    } catch (const interrupted&) {
      // what was decided stands; the rest is alive for want of a test
    }
  }
  for (const verdict v : r.verdicts)
    if (v == verdict::alive) ++r.alive;
  return r;
}

}  // namespace hsc::linear
