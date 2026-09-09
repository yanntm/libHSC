/// \file path.cc
/// \brief Layered search and backtrack (`hsc/trace/path.hh`, `algorithm.md` §1).
#include "hsc/trace/path.hh"

#include <algorithm>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"

namespace hsc::trace {

namespace {

using core::code;

/// The states of \p layer from which \p event leads into \p target, by the
/// forward test alone: split the layer arc by arc until one state remains.
/// Used only when the event has no converse.
code source_by_splitting(core::manager& mgr, const graph& g, code event,
                         code layer, code target) {
  core::diagram_engine& diagrams = mgr.diagrams();
  const auto hits = [&](code s) {
    return diagrams.meet(diagrams.apply_local(event, s), target) != core::none;
  };
  code cur = layer;
  for (;;) {
    const code one = g.one_state(cur);
    if (one == cur || hits(one)) return one;
    // Halve: the arcs of cur, the half that still hits.
    const std::span<const core::arc> arcs = diagrams.arcs(cur);
    if (arcs.size() > 1) {
      const std::size_t mid = arcs.size() / 2;
      std::vector<core::arc> a(arcs.begin(), arcs.begin() + mid);
      std::vector<core::arc> b(arcs.begin() + mid, arcs.end());
      const code ca = diagrams.canonize(diagrams.sort_of(cur), a);
      cur = hits(ca) ? ca : diagrams.canonize(diagrams.sort_of(cur), b);
      continue;
    }
    // One arc: descend into its sub by enumerating the head's one state.
    const core::arc x = arcs.front();
    const code head_one = g.one_state(diagrams.rectangle(diagrams.sort_of(cur), x.prime, x.sub));
    if (hits(head_one)) return head_one;
    // Fall back to the head part not chosen: remove that state and iterate.
    cur = diagrams.minus(cur, head_one);
    if (cur == core::none) return core::none;
  }
}

}  // namespace

std::optional<path_result> path(core::manager& mgr, const graph& g, code from,
                                code to, code constraint) {
  core::diagram_engine& diagrams = mgr.diagrams();
  core::op_table& ops = mgr.operations();
  if (from == core::none || to == core::none) return std::nullopt;
  if (g.within != core::none) from = diagrams.meet(from, g.within);
  if (from == core::none) return std::nullopt;

  // The step: the events after the constraint, fused where possible.
  std::vector<code> stepped;
  stepped.reserve(g.events.size());
  for (const code t : g.events) stepped.push_back(core::compose_at(mgr, g.sort, t, constraint));
  const code step = g.events.empty() ? core::none : core::sum_at(mgr, g.sort, stepped);

  // Forward layers until a target is reached.
  std::vector<code> layers{from};
  code seen = from;
  code reached = diagrams.meet(from, to);
  while (reached == core::none) {
    mgr.check_interrupt();
    if (step == core::none) return std::nullopt;
    code img = diagrams.apply_local(step, layers.back());
    if (g.within != core::none) img = diagrams.meet(img, g.within);
    const code fresh = diagrams.minus(img, seen);
    if (fresh == core::none) return std::nullopt;
    layers.push_back(fresh);
    seen = diagrams.join(seen, fresh);
    reached = diagrams.meet(fresh, to);
  }

  // Backtrack, one state at a time.
  path_result out;
  const std::size_t k = layers.size() - 1;
  code cur = g.one_state(reached);
  out.states.assign(k + 1, core::none);
  out.events.assign(k, 0);
  out.states[k] = cur;
  for (std::size_t i = k; i > 0; --i) {
    const code prev_layer = layers[i - 1];
    bool found = false;
    for (std::size_t j = 0; j < g.events.size() && !found; ++j) {
      code src = core::none;
      if (j < g.preds.size()) {
        src = diagrams.meet(diagrams.apply_local(g.preds[j], cur), prev_layer);
        // the constraint held on the source, a predecessor state
        if (src != core::none && constraint != core::op_table::id)
          src = diagrams.apply_local(constraint, src);
        if (src != core::none) src = g.one_state(src);
      } else {
        const code lim = constraint == core::op_table::id ? prev_layer
                                                          : diagrams.apply_local(constraint, prev_layer);
        if (lim != core::none &&
            diagrams.meet(diagrams.apply_local(g.events[j], lim), cur) != core::none) {
          src = source_by_splitting(mgr, g, g.events[j], lim, cur);
        }
      }
      if (src != core::none) {
        out.states[i - 1] = src;
        out.events[i - 1] = j;
        cur = src;
        found = true;
      }
    }
    if (!found) return std::nullopt;  // cannot happen: the layer was built from its predecessor
  }
  (void)ops;
  return out;
}

}  // namespace hsc::trace
