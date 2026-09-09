/// \file surface_cover.cc
/// \brief `(pump NAME [LEAF])`: a symbolic unboundedness witness — a pumping
/// pair on a shortest path (`sched/algorithm.md` §3b, the divergence watch).
///
/// For a Petri net a reachable `m` and a run `m →σ m'` with `m' ≥ m`
/// componentwise and `m'(p) > m(p)` prove `p` unbounded: `σ` fires again from
/// `m'` by monotonicity. The set NAME may be partial (every state in it is
/// reachable). The leaf is LEAF, or the one that ran furthest above its
/// initial value; X its largest value in NAME; a shortest path from the seed
/// to the states at X, inside NAME, is scanned for a pair `i < j` with
/// `m_j ≥ m_i` and the leaf strictly up. Prints `NAME pump LEAF X i j` and
/// the two states, or `NAME pump LEAF X none K` (a path of K steps, no pair).
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "hsc/core/operation.hh"
#include "hsc/trace/path.hh"
#include "surface_translator.hh"

namespace hsc::surface {

void translator::do_pump(const datum& form) {
  if (top_ == core::none) fail(form, "pump before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  try {
    pump(form, name);
  } catch (const interrupted&) {
    out_ << name << " pump none (deadline)\n";
  }
}

void translator::pump(const datum& form, const std::string& name) {
  const code set = named(form.items()[1]);
  if (set == core::none) {
    out_ << name << " pump none (empty set)\n";
    return;
  }
  // the largest value of every leaf in the set
  std::vector<std::int32_t> mx(order_.size(), std::numeric_limits<std::int32_t>::min());
  {
    std::unordered_map<core::shape_code, std::size_t> width;
    const auto span = [&](auto&& self, core::shape_code s) -> std::size_t {
      switch (mgr_.shapes().kind(s)) {
        case core::shape_kind::unit: return 0;
        case core::shape_kind::leaf: return 1;
        case core::shape_kind::pair: {
          if (const auto it = width.find(s); it != width.end()) return it->second;
          const std::size_t w = self(self, mgr_.shapes().head(s)) + self(self, mgr_.shapes().tail(s));
          width[s] = w;
          return w;
        }
      }
      return 0;
    };
    span(span, top_);
    std::unordered_set<std::uint64_t> seen;
    const auto visit = [&](auto&& self, code n, core::shape_code s, std::size_t first) -> void {
      if (n == core::none) return;
      switch (mgr_.shapes().kind(s)) {
        case core::shape_kind::unit: return;
        case core::shape_kind::leaf:
          for (const std::int32_t v : theory_->elements(n)) mx[first] = std::max(mx[first], v);
          return;
        case core::shape_kind::pair: {
          if (!seen.insert((static_cast<std::uint64_t>(n) << 20) ^ first).second) return;
          const core::shape_code hs = mgr_.shapes().head(s), ts = mgr_.shapes().tail(s);
          const std::size_t hw = mgr_.shapes().kind(hs) == core::shape_kind::leaf ? 1 : width[hs];
          for (const core::arc& a : mgr_.diagrams().arcs(n)) {
            self(self, a.prime, hs, first);
            self(self, a.sub, ts, first + hw);
          }
          return;
        }
      }
    };
    visit(visit, set, top_, 0);
  }
  // The leaves to try: the given one, or every leaf above its initial value,
  // the furthest first. For each, targets close to the initial value first
  // (a path to the leaf's largest value is as long as that value; the loop
  // that pumps shows up on the way to initial + 1 or + 2), then geometric.
  std::vector<std::int32_t> init_values;
  first_word(top_, seed(), init_values);
  std::vector<std::size_t> leaves;
  if (form.items().size() > 2) {
    const std::optional<std::uint32_t> pos = position(sym(form.items()[2]));
    if (!pos) fail(form.items()[2], "pump: unknown leaf");
    leaves.push_back(*pos);
  } else {
    for (std::size_t i = 0; i < order_.size(); ++i)
      if (mx[i] > (i < init_values.size() ? init_values[i] : 0)) leaves.push_back(i);
    std::ranges::sort(leaves, [&](std::size_t a, std::size_t b) {
      return mx[a] - init_values[a] > mx[b] - init_values[b];
    });
    if (leaves.size() > 4) leaves.resize(4);
  }
  // The search runs on a small set — the seed's neighbourhood, a few naive
  // layers — not on the (possibly enormous) set that gave the maxima: a pump
  // shows up close to the seed, and selections, inversions and layers over
  // billions of states would eat the budget before the first path.
  core::diagram_engine& diagrams = mgr_.diagrams();
  const code all = core::sum_at(mgr_, top_, events_);
  code near = seed();
  for (int layer = 0; layer < 24; ++layer) {
    if (mgr_.stopping()) break;
    const code grown = diagrams.join(near, diagrams.apply_local(all, near));
    if (grown == near) break;
    near = grown;
  }
  std::vector<code> preds;
  try {
    core::inverter inv(mgr_);
    for (const code ev : events_) preds.push_back(inv(top_, ev, near));
  } catch (const unsupported_error&) {
    preds.clear();
  }
  trace::graph g;
  g.sort = top_;
  g.events = events_;
  g.preds = preds;
  g.within = near;
  g.one_state = [this](code s) -> code {
    std::vector<std::int32_t> values;
    if (!first_word(top_, s, values)) return core::none;
    std::size_t next = 0;
    return build_point(top_, next, values);
  };
  std::size_t searches = 0;
  for (const std::size_t p : leaves) {
    const std::int32_t init = p < init_values.size() ? init_values[p] : 0;
    for (std::int32_t X = init + 1; X <= mx[p] && searches < 12; X = X < init + 3 ? X + 1 : init + 2 * (X - init)) {
      ++searches;
      const datum at_x = datum::list({datum::atom("==", form.line()), datum::atom(order_[p], form.line()),
                                      datum::atom(std::to_string(X), form.line())}, form.line());
      const code target = apply_atom(at_x, near);
      if (target == core::none) continue;
      std::optional<trace::path_result> path;
      try {
        path = trace::path(mgr_, g, seed(), target, core::op_table::id);
      } catch (const interrupted&) {  // the pump's budget ran out inside a search
        out_ << name << " pump none (deadline after " << searches << " paths)\n";
        return;
      }
      if (!path) continue;
      std::vector<std::vector<std::int32_t>> words(path->states.size());
      for (std::size_t i = 0; i < words.size(); ++i) first_word(top_, path->states[i], words[i]);
      for (std::size_t j = 1; j < words.size(); ++j) {
        for (std::size_t i = 0; i < j; ++i) {
          if (words[j][p] <= words[i][p]) continue;
          bool dominates = true;
          for (std::size_t k = 0; k < words[i].size() && dominates; ++k) dominates = words[j][k] >= words[i][k];
          if (!dominates) continue;
          out_ << name << " pump " << order_[p] << ' ' << X << ' ' << i << ' ' << j << '\n';
          out_ << "  ";
          print_word(words[i]);
          out_ << "\n  ";
          print_word(words[j]);
          out_ << "\n  ; " << (j - i) << " steps:";
          for (std::size_t k = i; k < j; ++k) out_ << ' ' << event_names_[path->events[k]];
          out_ << '\n';
          return;
        }
      }
    }
  }
  out_ << name << " pump none " << searches << " paths searched\n";
}

}  // namespace hsc::surface
