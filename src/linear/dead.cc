/// \file dead.cc
/// \brief The two deadness tests (`hsc/linear/dead.hh`).
#include "hsc/linear/dead.hh"

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"

namespace hsc::linear {

dead_report dead_transitions(core::manager& mgr, core::shape_code, core::code set, core::code init,
                             std::span<const core::code> events, std::span<const core::code> guards,
                             core::code step, bool with_step, std::size_t sharpen) {
  core::diagram_engine& d = mgr.diagrams();
  dead_report r;
  r.verdicts.assign(events.size(), verdict::alive);
  // one image of the set serves the cheap test of every transition
  const core::code image = (with_step && set != core::none) ? d.apply_local(step, set) : core::none;
  std::vector<std::size_t> survivors;
  for (std::size_t i = 0; i < events.size(); ++i) {
    if (i >= guards.size() || guards[i] == core::none) {
      r.verdicts[i] = verdict::untested;
      ++r.untested;
      continue;
    }
    const core::code en = d.apply_local(guards[i], set);
    if (en == core::none) {
      r.verdicts[i] = verdict::never_enabled;
      ++r.never_enabled;
      continue;
    }
    if (!with_step) continue;
    const bool initially = d.apply_local(guards[i], init) != core::none;
    if (!initially && d.meet(en, image) == core::none) {
      r.verdicts[i] = verdict::one_step;
      ++r.one_step;
      continue;
    }
    if (!initially) survivors.push_back(i);
  }
  // the exact one-step test for the survivors, while they are few: the image
  // of the markings that do not enable t, one per transition
  if (survivors.size() <= sharpen) {
    for (const std::size_t i : survivors) {
      const core::code en = d.apply_local(guards[i], set);
      const core::code from = d.minus(set, en);
      const core::code img = from == core::none ? core::none : d.apply_local(step, from);
      if (d.meet(en, img) == core::none) {
        r.verdicts[i] = verdict::one_step;
        ++r.one_step;
      }
    }
  }
  for (const verdict v : r.verdicts)
    if (v == verdict::alive) ++r.alive;
  return r;
}

}  // namespace hsc::linear
