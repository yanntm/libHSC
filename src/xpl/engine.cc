/// \file engine.cc
/// \brief BFS with may-fire set maintenance, visitor-driven.
/// `algorithm.md` §2–§3.

#include "hsc/xpl/engine.hh"

#include <deque>
#include <limits>
#include <utility>

#include "hsc/xpl/interpret/fire.hh"

namespace hsc::xpl {

namespace {

/// The one full sweep a seed pays.
SparseBoolArray full_sweep(const model& m, state_view s) {
  SparseBoolArray en;
  for (std::uint32_t e = 0; e < m.events.size(); ++e) {
    if (quick_enabled(m, e, s)) en.append(e, true);
  }
  return en;
}

/// E' from E: dirty events re-evaluated on \p s, clean ones keep their
/// status — `quick` is a function of ctrl, and only `changed` moved.
SparseBoolArray derive(const model& m, const SparseBoolArray& en,
                       const std::vector<std::uint32_t>& changed,
                       state_view s) {
  SparseBoolArray dirty;
  for (const std::uint32_t p : changed) {
    dirty = SparseBoolArray::unionOperation(dirty, m.readers[p]);
  }
  SparseBoolArray out;
  constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
  std::size_t a = 0;
  std::size_t b = 0;
  while (a < en.size() || b < dirty.size()) {
    const std::size_t ka = a < en.size() ? en.keyAt(a) : none;
    const std::size_t kb = b < dirty.size() ? dirty.keyAt(b) : none;
    const std::size_t id = ka < kb ? ka : kb;
    if (kb <= ka) {  // dirty: re-evaluate, in the set or not before
      if (quick_enabled(m, static_cast<std::uint32_t>(id), s)) {
        out.append(id, true);
      }
    } else {  // clean and was in the set: stays
      out.append(id, true);
    }
    if (ka == id) ++a;
    if (kb == id) ++b;
  }
  return out;
}

/// The search loop over a caller-owned store. \p on_state may be empty.
explore_stats run(const model& m, std::span<const word> seeds,
                  const state_visitor& on_state, const reach_options& opt,
                  state_store& store) {
  explore_stats r;
  std::deque<std::pair<state_id, SparseBoolArray>> frontier;
  // intern → cap check → visitor → queue, seeds and successors alike
  const auto admit = [&](state_id id,
                         const SparseBoolArray& en) -> explore_stats::status {
    if (store.size() > opt.cap) {
      r.diagnostic = "state cap " + std::to_string(opt.cap) + " exceeded";
      return explore_stats::status::capped;
    }
    if (on_state && on_state(id, store[id]) == visit::stop) {
      return explore_stats::status::stopped;
    }
    frontier.emplace_back(id, en);
    return explore_stats::status::ok;
  };
  try {
    for (const word& s : seeds) {
      const auto [id, fresh] = store.intern(s);
      if (!fresh) continue;
      if (const auto st = admit(id, full_sweep(m, s));
          st != explore_stats::status::ok) {
        r.st = st;
        r.count = store.size();
        return r;
      }
    }
    std::vector<successor> succs;
    while (!frontier.empty()) {
      const auto [id, en] = std::move(frontier.front());
      frontier.pop_front();
      const word s(store[id].begin(), store[id].end());
      for (std::size_t i = 0; i < en.size(); ++i) {
        const auto e = static_cast<std::uint32_t>(en.keyAt(i));
        succs.clear();
        fire(m, e, s, succs);
        r.fired += succs.size();
        for (successor& sc : succs) {
          const auto [nid, fresh] = store.intern(sc.s);
          if (!fresh) continue;
          if (const auto st = admit(nid, derive(m, en, sc.changed, sc.s));
              st != explore_stats::status::ok) {
            r.st = st;
            r.count = store.size();
            return r;
          }
        }
      }
    }
    r.st = explore_stats::status::ok;
  } catch (const interp_error& err) {
    const event& evt = m.events[err.event()];
    r.st = explore_stats::status::error;
    r.diagnostic = "event " + evt.name + " (line " +
                   std::to_string(evt.line) + "): " + err.what();
    r.witness = err.at();
  }
  r.count = store.size();
  return r;
}

}  // namespace

explore_stats explore(const model& m, std::span<const word> seeds,
                      const state_visitor& on_state,
                      const reach_options& opt) {
  state_store store(m.arity);
  return run(m, seeds, on_state, opt, store);
}

reach_result reach(const model& m, std::span<const word> seeds,
                   const reach_options& opt) {
  reach_result r{explore_stats{}, state_store(m.arity)};
  r.stats = run(m, seeds, state_visitor{}, opt, r.states);
  return r;
}

}  // namespace hsc::xpl
