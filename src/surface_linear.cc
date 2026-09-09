/// \file surface_linear.cc
/// \brief The bindings of `hsc/linear/`: `(full NAME [(LEAF LO HI)]*)` and
/// `(dead NAME SET)` (`include/hsc/linear/algorithm.md`).
///
/// `full` binds the product of the leaf domains — each leaf's declared
/// domain, or the one given for it. `dead` tests every event of the default
/// system against SET: never enabled on it, or enabled in one step only from
/// markings SET rules out (the initial states not enabling it); an event
/// without a recorded guard (a family, a no-op) is untested. One line per
/// event that is dead, then a summary.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include "hsc/core/operation.hh"
#include "hsc/linear/dead.hh"
#include "hsc/linear/equality.hh"
#include "hsc/linear/full.hh"
#include "surface_translator.hh"

namespace hsc::surface {

void translator::do_full(const datum& form) {
  if (top_ == core::none) fail(form, "full before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  std::vector<std::pair<std::int32_t, std::int32_t>> range(order_.size(), {0, -1});
  for (std::size_t i = 0; i < order_.size(); ++i) {
    const auto it = leaves_.find(order_[i]);
    if (it != leaves_.end() && it->second.bounded) range[i] = {it->second.lo, it->second.hi - 1};
  }
  for (std::size_t k = 2; k < form.items().size(); ++k) {
    const datum& t = form.items()[k];
    if (!t.is_list() || t.items().size() != 3) fail(t, "full takes (LEAF LO HI) triples");
    const std::optional<std::uint32_t> pos = position(sym(t.items()[0]));
    if (!pos) fail(t.items()[0], "full: unknown leaf");
    range[*pos] = {static_cast<std::int32_t>(std::stol(t.items()[1].text())),
                   static_cast<std::int32_t>(std::stol(t.items()[2].text()))};
  }
  for (std::size_t i = 0; i < range.size(); ++i)
    if (range[i].second < range[i].first) fail(form, "full: leaf " + order_[i] + " has no domain (declare a bound, or give one)");
  results_[name] = linear::full_box(mgr_, top_, [&](std::size_t p) -> code {
    return theory_->interval(range[p].first, range[p].second + 1);
  });
  out_ << name << " full " << mgr_.diagrams().cardinal(results_[name]) << '\n';
}

/// `(equality NAME BOX K (* C LEAF)*)`: the words of BOX (a product, `full`)
/// that satisfy the equality, built directly (`linear/equality.hh`); the
/// domains are BOX's own, so the set is exactly `BOX ∩ {Σ C·LEAF = K}`;
/// coefficients nonnegative.
void translator::do_equality(const datum& form) { linear_constraint(form, false); }
void translator::do_at_most(const datum& form) { linear_constraint(form, true); }

/// `(equality NAME BOX K (* C LEAF)*)` / `(at-most NAME BOX K (* C LEAF)*)`:
/// the words of BOX with `Σ C·LEAF = K`, or `≤ K`, built directly.
void translator::linear_constraint(const datum& form, bool at_most) {
  if (top_ == core::none) fail(form, "equality before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code box = named(arg(form, 2, "box"));
  const long long k = std::stoll(arg(form, 3, "constant").text());
  std::vector<long long> coeff(order_.size(), 0);
  for (std::size_t i = 4; i < form.items().size(); ++i) {
    const datum& t = form.items()[i];
    if (!t.is_list() || t.items().size() != 3 || t.items()[0].text() != "*") fail(t, "equality takes terms (* C LEAF)");
    const std::optional<std::uint32_t> pos = position(sym(t.items()[2]));
    if (!pos) fail(t.items()[2], "equality: unknown leaf");
    coeff[*pos] += std::stoll(t.items()[1].text());
  }
  // the domains: the box's leaf primes, read down its (single) path of products
  std::vector<std::vector<std::int32_t>> domains(order_.size());
  {
    const core::shape_table& sh = mgr_.shapes();
    core::diagram_engine& d = mgr_.diagrams();
    std::unordered_map<core::shape_code, std::size_t> width;
    const auto span = [&](auto&& self, core::shape_code s) -> std::size_t {
      switch (sh.kind(s)) {
        case core::shape_kind::unit: return 0;
        case core::shape_kind::leaf: return 1;
        case core::shape_kind::pair: {
          if (const auto it = width.find(s); it != width.end()) return it->second;
          const std::size_t w = self(self, sh.head(s)) + self(self, sh.tail(s));
          width[s] = w;
          return w;
        }
      }
      return 0;
    };
    span(span, top_);
    const auto read = [&](auto&& self, code n, core::shape_code s, std::size_t first) -> void {
      if (n == core::none || sh.kind(s) != core::shape_kind::pair) return;
      const core::shape_code hs = sh.head(s), ts = sh.tail(s);
      const std::size_t hw = sh.kind(hs) == core::shape_kind::leaf ? 1 : width[hs];
      for (const core::arc& a : d.arcs(n)) {
        if (sh.kind(hs) == core::shape_kind::leaf) {
          for (const std::int32_t v : theory_->elements(a.prime)) domains[first].push_back(v);
        } else {
          self(self, a.prime, hs, first);
        }
        self(self, a.sub, ts, first + hw);
      }
    };
    read(read, box, top_, 0);
    for (auto& dom : domains) {
      std::ranges::sort(dom);
      dom.erase(std::unique(dom.begin(), dom.end()), dom.end());
    }
  }
  linear::leaf_access acc;
  acc.values = [&](std::size_t p) -> std::span<const std::int32_t> { return domains[p]; };
  acc.subset = [&](std::size_t, std::span<const std::int32_t> vs) -> code { return theory_->of(vs); };
  results_[name] = linear::equality(mgr_, top_, coeff, k, acc, at_most);
  out_ << name << (at_most ? " at-most " : " equality ") << (results_[name] == core::none ? 0.0 : mgr_.diagrams().cardinal(results_[name])) << '\n';
}

/// `(intersect NAME A B*)`: the meet of bound results.
void translator::do_intersect(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  code r = named(arg(form, 2, "a result"));
  for (std::size_t i = 3; i < form.items().size() && r != core::none; ++i) r = mgr_.diagrams().meet(r, named(form.items()[i]));
  results_[name] = r;
  out_ << name << " intersect " << (r == core::none ? 0.0 : mgr_.diagrams().cardinal(r)) << '\n';
}

/// `(dead NAME SET [step] [ignore LEAF*])`: the leaves after `ignore` are the
/// ones SET only caps (no bound known): the guard atoms that read them are
/// dropped — a weaker guard, so "never enabled" stays sound — and the one-step
/// test, which needs SET exact where the transition looks, skips the events
/// that read them.
void translator::do_dead(const datum& form) {
  if (top_ == core::none) fail(form, "dead before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code set = named(arg(form, 2, "set"));
  bool with_step = false;
  std::size_t depth = 1;  // layers of the backward search from a slice
  std::unordered_set<std::string> ignored;
  for (std::size_t i = 3, mode = 0; i < form.items().size(); ++i) {
    const datum& d = form.items()[i];
    if (d.is_atom() && d.text() == "step") { with_step = true; mode = 2; }
    else if (d.is_atom() && d.text() == "ignore") mode = 1;
    else if (mode == 2 && d.is_atom() && std::isdigit(static_cast<unsigned char>(d.text()[0]))) { depth = std::stoul(d.text()); mode = 0; }
    else if (mode == 1 && d.is_atom()) ignored.insert(d.text());
    else fail(d, "dead takes NAME SET [step [K]] [ignore LEAF*]");
  }
  const auto reads_ignored = [&](const datum& atom) {
    bool r = false;
    const auto walk = [&](auto&& self, const datum& x) -> void {
      if (x.is_atom()) { if (ignored.count(x.text())) r = true; return; }
      for (const datum& y : x.items()) self(self, y);
    };
    walk(walk, atom);
    return r;
  };
  // the enabling selector of every event: its guard atoms as one (when …),
  // the atoms on ignored leaves left out; `exact` says none was left out
  std::vector<code> guards(events_.size(), core::none);
  std::vector<char> exact(events_.size(), 1);
  for (std::size_t i = 0; i < events_.size(); ++i) {
    const std::size_t g = i < event_guard_of_.size() ? event_guard_of_[i] : SIZE_MAX;
    if (g == SIZE_MAX || g >= event_guards_.size()) continue;
    std::vector<datum> when{datum::atom("when", form.line())};
    for (const datum& a : event_guards_[g]) {
      if (reads_ignored(a)) exact[i] = 0; else when.push_back(a);
    }
    guards[i] = read_evterm(datum::list(std::move(when), form.line()));
  }
  // the one-step test backward: the converses against the set, restricted per
  // slice to the live events writing a leaf the guard reads
  // per event, the leaves its guard reads and the direction a move must
  // take to enter the guard: an increase for `>=`/`>`, a decrease for
  // `<=`/`<`, either for anything else
  std::vector<std::unordered_map<std::string, int>> reads(events_.size());
  for (std::size_t i = 0; i < events_.size(); ++i) {
    const std::size_t g = i < event_guard_of_.size() ? event_guard_of_[i] : SIZE_MAX;
    if (g == SIZE_MAX || g >= event_guards_.size()) continue;
    for (const datum& a : event_guards_[g]) {
      const std::string op = a.is_list() && !a.items().empty() ? a.head() : "";
      const int dir = (op == ">=" || op == ">") ? 1 : (op == "<=" || op == "<") ? -1 : 0;
      const auto walk = [&](auto&& self, const datum& x) -> void {
        if (x.is_atom()) {
          if (!position(x.text())) return;
          auto [it, fresh] = reads[i].emplace(x.text(), dir);
          if (!fresh && it->second != dir) it->second = 0;
          return;
        }
        for (const datum& y : x.items()) self(self, y);
      };
      walk(walk, a);
    }
  }
  linear::entry_fn entries;
  const auto t0 = std::chrono::steady_clock::now();
  const auto since = [t0]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };
  std::size_t tested = 0, killed = 0;
  if (with_step) {
    const std::vector<code>& conv = converses_against(form, set);
    out_ << name << " dead-converses " << conv.size() << " in " << since() << " s\n";
    // The entries of a slice, traced up to `depth` layers back inside the set:
    // the first layer under the events that can enter the guard, the next
    // ones under every live event. A layer meeting the initial marking is a
    // path (the slice is reachable: alive); a layer empty before that closes
    // the search (nothing of the set leads to the slice: dead, `none`
    // returned); otherwise the last layer stands for "unknown".
    entries = [&, conv](std::size_t i, code en, const std::vector<char>& live) -> code {
      core::diagram_engine& d = mgr_.diagrams();
      std::vector<std::size_t> which;
      for (const std::size_t u : events_entering(reads[i])) if (live[u]) which.push_back(u);
      std::vector<std::size_t> all_live;
      if (depth > 1) for (std::size_t u = 0; u < events_.size(); ++u) if (live[u]) all_live.push_back(u);
      code seen = en, frontier = en, r = core::none;
      for (std::size_t k = 0; k < depth; ++k) {
        const code pre = pre_within(frontier, set, conv, k == 0 ? std::span<const std::size_t>(which) : std::span<const std::size_t>(all_live));
        const code fresh = pre == core::none ? core::none : d.minus(pre, seen);
        if (fresh == core::none) { r = core::none; break; }
        r = fresh;
        if (d.meet(fresh, seed()) != core::none) break;  // a real path into the slice
        seen = d.join(seen, fresh);
        frontier = fresh;
      }
      if (r == core::none) ++killed;
      if (++tested % 100 == 0)
        out_ << name << " dead-progress tested " << tested << " killed " << killed << " events " << which.size()
             << " at " << since() << " s\n";
      return r;
    };
  }
  const linear::dead_report r =
      linear::dead_transitions(mgr_, top_, set, seed(), events_, guards, exact, entries);
  for (std::size_t i = 0; i < events_.size(); ++i) {
    if (r.verdicts[i] == linear::verdict::never_enabled) out_ << name << " dead " << event_names_[i] << " never\n";
    else if (r.verdicts[i] == linear::verdict::one_step) out_ << name << " dead " << event_names_[i] << " step\n";
  }
  out_ << name << " dead-summary never " << r.never_enabled << " step " << r.one_step << " alive " << r.alive
       << " untested " << r.untested << " candidates " << r.candidates << " rounds " << r.rounds
       << (r.stopped ? " stopped" : "") << '\n';
}

}  // namespace hsc::surface
