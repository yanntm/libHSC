/// \file surface_backward.cc
/// \brief Backward steps within a bound set: `(pre …)`, `(minus …)`,
/// `(backward …)` — the impossibility tests of `hsc/linear/algorithm.md` §2
/// read backward. The converses of the default system are inverted against
/// the set they run in (the potential, `core/algorithm.md` §9) and kept per
/// set; no protection is applied, every predecessor is met with the set.
#include <algorithm>
#include <unordered_set>

#include "hsc/core/diagram.hh"
#include "hsc/core/operation.hh"
#include "surface_translator.hh"

namespace hsc::surface {

/// The raw converses of the default events against \p potential, in the
/// events' order, computed once per potential.
const std::vector<code>& translator::converses_against(const datum& at, code potential) {
  const auto it = converses_.find(potential);
  if (it != converses_.end()) return it->second;
  core::inverter inv(mgr_);
  std::vector<code> preds;
  preds.reserve(events_.size());
  for (const code ev : events_) {
    try {
      preds.push_back(inv(top_, ev, potential));
    } catch (const unsupported_error& e) {
      fail(at, std::string("cannot invert the default system: ") + e.what());
    }
  }
  return converses_.emplace(potential, std::move(preds)).first->second;
}

/// The events of the default system that write one of \p leaves (an event
/// whose writes are unknown — a family — is kept). Empty \p leaves: all.
std::vector<std::size_t> translator::events_writing(const std::unordered_set<std::string>& leaves) {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < events_.size(); ++i) {
    if (leaves.empty() || i >= event_writes_.size() || event_writes_[i].empty()) { out.push_back(i); continue; }
    for (const auto& [w, dir] : event_writes_[i])
      if (leaves.count(w)) { out.push_back(i); break; }
  }
  return out;
}

std::vector<std::size_t> translator::events_entering(const std::unordered_map<std::string, int>& wanted) {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < events_.size(); ++i) {
    if (wanted.empty() || i >= event_writes_.size() || event_writes_[i].empty()) { out.push_back(i); continue; }
    for (const auto& [w, dir] : event_writes_[i]) {
      const auto it = wanted.find(w);
      if (it != wanted.end() && (it->second == 0 || dir == 0 || dir == it->second)) { out.push_back(i); break; }
    }
  }
  return out;
}

/// `pre(X) ∩ set` under the listed events' converses, each predecessor set
/// met with \p set before the join. Throws `interrupted` when stopped.
code translator::pre_within(code x, code set, std::span<const code> conv, std::span<const std::size_t> which) {
  core::diagram_engine& d = mgr_.diagrams();
  code acc = core::none;
  for (const std::size_t i : which) {
    if (mgr_.stopping()) throw interrupted("backward step stopped");
    const code p = d.meet(d.apply_local(conv[i], x), set);
    if (p == core::none) continue;
    acc = acc == core::none ? p : d.join(acc, p);
  }
  return acc;
}

/// Parse the trailing `[steps K] [writing LEAF*]` of a form from item \p from.
static void read_back_options(const datum& form, std::size_t from, std::size_t& steps,
                              std::unordered_set<std::string>& writing) {
  for (std::size_t i = from, mode = 0; i < form.items().size(); ++i) {
    const datum& d = form.items()[i];
    if (d.is_atom() && d.text() == "steps") mode = 1;
    else if (d.is_atom() && d.text() == "writing") mode = 2;
    else if (mode == 1 && d.is_atom()) { steps = std::stoul(d.text()); mode = 0; }
    else if (mode == 2 && d.is_atom()) writing.insert(d.text());
    else throw std::runtime_error("unexpected item in a backward form");
  }
}

/// `(pre NAME X SET [writing LEAF*])`: the predecessors of X inside SET
/// under the default system — restricted to the events writing one of the
/// listed leaves when given. Prints `NAME pre COUNT`, or `NAME partial`
/// when stopped (the result then under-approximates: not to be trusted).
void translator::do_pre(const datum& form) {
  if (top_ == core::none) fail(form, "pre before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code x = named(arg(form, 2, "source set"));
  const code set = named(arg(form, 3, "potential set"));
  std::size_t steps = 0;
  std::unordered_set<std::string> writing;
  try { read_back_options(form, 4, steps, writing); } catch (const std::runtime_error& e) { fail(form, e.what()); }
  const std::vector<code>& conv = converses_against(form, set);
  const std::vector<std::size_t> which = events_writing(writing);
  try {
    results_[name] = pre_within(x, set, conv, which);
    out_ << name << " pre " << (results_[name] == core::none ? 0.0 : mgr_.diagrams().cardinal(results_[name])) << '\n';
  } catch (const interrupted&) {
    results_[name] = core::none;
    out_ << name << " partial\n";
  }
}

/// `(minus NAME A B)`: the states of A not in B.
void translator::do_minus(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  const code a = named(arg(form, 2, "set"));
  const code b = named(arg(form, 3, "set"));
  results_[name] = mgr_.diagrams().minus(a, b);
  out_ << name << " minus " << (results_[name] == core::none ? 0.0 : mgr_.diagrams().cardinal(results_[name])) << '\n';
}

/// `(backward NAME X SET [steps K] [writing LEAF*])`: the states of SET from
/// which X is reached, by layers — `X`, then its predecessors inside SET not
/// seen, and so on. Stops with `NAME backward init K` as soon as a layer
/// meets the initial states (X is reachable: the converses are exact, the
/// path is real), `NAME backward closed K` when a layer is empty (nothing
/// of SET outside the result leads to X), `NAME backward open K` after K
/// layers, `NAME backward partial K` when stopped. The first layer alone
/// may be restricted to the events writing the listed leaves: entering X
/// from outside changes a leaf X reads. NAME is bound to the union.
void translator::do_backward(const datum& form) {
  if (top_ == core::none) fail(form, "backward before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code x = named(arg(form, 2, "source set"));
  const code set = named(arg(form, 3, "potential set"));
  std::size_t steps = 0;
  std::unordered_set<std::string> writing;
  try { read_back_options(form, 4, steps, writing); } catch (const std::runtime_error& e) { fail(form, e.what()); }
  core::diagram_engine& d = mgr_.diagrams();
  const std::vector<code>& conv = converses_against(form, set);
  const std::vector<std::size_t> all = events_writing({});
  const std::vector<std::size_t> first = events_writing(writing);
  code seen = d.meet(x, set), frontier = seen;
  std::size_t k = 0;
  const char* how = "open";
  try {
    for (;;) {
      if (d.meet(frontier, seed()) != core::none) { how = "init"; break; }
      if (steps != 0 && k >= steps) { how = "open"; break; }
      const code pre = pre_within(frontier, set, conv, k == 0 ? std::span<const std::size_t>(first) : std::span<const std::size_t>(all));
      const code fresh = pre == core::none ? core::none : d.minus(pre, seen);
      ++k;
      if (fresh == core::none) { how = "closed"; break; }
      seen = d.join(seen, fresh);
      frontier = fresh;
    }
  } catch (const interrupted&) {
    how = "partial";
  }
  results_[name] = seen;
  out_ << name << " backward " << how << ' ' << k << '\n';
}

/// The events of the default system a `(dead …)` result left alive, when the
/// form carries `alive NAME` from item \p from; all of them otherwise.
std::vector<std::size_t> translator::events_alive(const datum& form, std::size_t from) {
  const std::vector<char>* live = nullptr;
  for (std::size_t i = from; i + 1 < form.items().size(); ++i) {
    const datum& d = form.items()[i];
    if (d.is_atom() && d.text() == "alive") {
      const auto it = dead_live_.find(sym(form.items()[i + 1]));
      if (it == dead_live_.end()) fail(form.items()[i + 1], "alive: no such dead result");
      live = &it->second;
    }
  }
  std::vector<std::size_t> out;
  for (std::size_t u = 0; u < events_.size(); ++u)
    if (live == nullptr || (u < live->size() && (*live)[u])) out.push_back(u);
  return out;
}

/// `post(X) ∩ set` under the listed events, each image met with \p set
/// before the join. Throws `interrupted` when stopped.
code translator::post_within(code x, code set, std::span<const std::size_t> which) {
  core::diagram_engine& d = mgr_.diagrams();
  code acc = core::none;
  for (const std::size_t i : which) {
    if (mgr_.stopping()) throw interrupted("forward step stopped");
    const code p = d.meet(d.apply_local(events_[i], x), set);
    if (p == core::none) continue;
    acc = acc == core::none ? p : d.join(acc, p);
  }
  return acc;
}

/// `(post NAME X SET [alive D])`: the successors of X inside SET under the
/// default system — the events a dead result D left alive when given.
/// Prints `NAME post COUNT NODES`, or `NAME partial` when stopped.
void translator::do_post(const datum& form) {
  if (top_ == core::none) fail(form, "post before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code x = named(arg(form, 2, "source set"));
  const code set = named(arg(form, 3, "potential set"));
  const std::vector<std::size_t> which = events_alive(form, 4);
  try {
    results_[name] = post_within(x, set, which);
    out_ << name << " post " << (results_[name] == core::none ? 0.0 : mgr_.diagrams().cardinal(results_[name]))
         << ' ' << (results_[name] == core::none ? 0 : mgr_.diagrams().size(results_[name])) << " events " << which.size() << '\n';
  } catch (const interrupted&) {
    results_[name] = core::none;
    out_ << name << " partial\n";
  }
}

/// `(support NAME SET [alive D] [rounds K])`: the greatest fixpoint of
/// `X ↦ X ∩ (init ∪ post(X))` below SET — the markings of SET that a chain
/// of firings inside the set supports from the initial marking; a sound
/// over-approximation of the reachable set finer than SET (a cycle of
/// unreachable markings survives it). Forward images only. Prints
/// `NAME support closed|open|partial K COUNT NODES`; NAME is bound to the
/// last set.
void translator::do_support(const datum& form) {
  if (top_ == core::none) fail(form, "support before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code set = named(arg(form, 2, "set"));
  const std::vector<std::size_t> which = events_alive(form, 3);
  std::size_t rounds = 0;
  for (std::size_t i = 3; i + 1 < form.items().size(); ++i)
    if (form.items()[i].is_atom() && form.items()[i].text() == "rounds") rounds = std::stoul(form.items()[i + 1].text());
  core::diagram_engine& d = mgr_.diagrams();
  code cur = set;
  std::size_t k = 0;
  const char* how = "open";
  try {
    for (;;) {
      if (rounds != 0 && k >= rounds) { how = "open"; break; }
      const code img = post_within(cur, cur, which);
      const code next = d.meet(cur, img == core::none ? seed() : d.join(img, seed()));
      ++k;
      if (next == cur) { how = "closed"; break; }
      cur = next;
    }
  } catch (const interrupted&) {
    how = "partial";
  }
  results_[name] = cur;
  out_ << name << " support " << how << ' ' << k << ' ' << (cur == core::none ? 0.0 : d.cardinal(cur)) << ' '
       << (cur == core::none ? 0 : d.size(cur)) << '\n';
}

}  // namespace hsc::surface
