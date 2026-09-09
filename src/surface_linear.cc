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
#include <unordered_map>

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
void translator::do_equality(const datum& form) {
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
    const long long c = std::stoll(t.items()[1].text());
    if (c < 0) fail(t, "equality: coefficients must be nonnegative");
    coeff[*pos] += c;
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
  results_[name] = linear::equality(mgr_, top_, coeff, k, acc);
  out_ << name << " equality " << (results_[name] == core::none ? 0.0 : mgr_.diagrams().cardinal(results_[name])) << '\n';
}

/// `(intersect NAME A B*)`: the meet of bound results.
void translator::do_intersect(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  code r = named(arg(form, 2, "a result"));
  for (std::size_t i = 3; i < form.items().size() && r != core::none; ++i) r = mgr_.diagrams().meet(r, named(form.items()[i]));
  results_[name] = r;
  out_ << name << " intersect " << (r == core::none ? 0.0 : mgr_.diagrams().cardinal(r)) << '\n';
}

void translator::do_dead(const datum& form) {
  if (top_ == core::none) fail(form, "dead before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code set = named(arg(form, 2, "set"));
  // the enabling selector of every event: its guard atoms as one (when …)
  std::vector<code> guards(events_.size(), core::none);
  for (std::size_t i = 0; i < events_.size(); ++i) {
    const std::size_t g = i < event_guard_of_.size() ? event_guard_of_[i] : SIZE_MAX;
    if (g == SIZE_MAX || g >= event_guards_.size()) continue;
    std::vector<datum> when{datum::atom("when", form.line())};
    for (const datum& a : event_guards_[g]) when.push_back(a);
    guards[i] = read_evterm(datum::list(std::move(when), form.line()));
  }
  const bool with_step = form.items().size() > 3 && form.items()[3].is_atom() && form.items()[3].text() == "step";
  const code step = (with_step && !events_.empty()) ? core::sum_at(mgr_, top_, events_) : core::none;
  const linear::dead_report r =
      linear::dead_transitions(mgr_, top_, set, seed(), events_, guards, step, with_step, /*sharpen=*/256);
  for (std::size_t i = 0; i < events_.size(); ++i) {
    if (r.verdicts[i] == linear::verdict::never_enabled) out_ << name << " dead " << event_names_[i] << " never\n";
    else if (r.verdicts[i] == linear::verdict::one_step) out_ << name << " dead " << event_names_[i] << " step\n";
  }
  out_ << name << " dead-summary never " << r.never_enabled << " step " << r.one_step << " alive " << r.alive
       << " untested " << r.untested << '\n';
}

}  // namespace hsc::surface
