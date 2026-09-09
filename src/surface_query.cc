/// \file surface_query.cc
/// \brief The symbolic commands: reach and the event-term readers,
/// select and its crossing atoms, counting, exhibition of states as
/// word literals, expectations.

#include <algorithm>
#include <cstdint>

#include <unordered_map>
#include <unordered_set>
#include "surface_translator.hh"

namespace hsc::surface {

code translator::named(const datum& d) {
  const std::string& name = sym(d);
  auto it = results_.find(name);
  if (it == results_.end()) fail(d, "no result named '" + name + "'");
  return it->second;
}

/// The least fixpoint of \p system (default: the ALT of every declared
/// event) from \p from (default: the seed). `naive` iterates, `saturate`
/// applies the static closure over the flattened summands; both denote
/// the same diagram. Propagates `hsc::overflow_error` if a leaf value
/// leaves its representable range.
code translator::run_reach(bool naive, std::optional<code> system,
               std::optional<code> from) {
  std::vector<code> summands;
  if (system) {
    collect_summands(*system, summands);
  } else {
    summands = events_;
  }
  core::diagram_engine& diagrams = mgr_.diagrams();
  code reachable = from ? *from : seed();
  util::stopwatch sw;
  if (naive) {
    const code all = core::sum_at(mgr_, top_, summands);
    for (;;) {
      const code grown =
          diagrams.join(reachable, diagrams.apply_local(all, reachable));
      if (grown == reachable) break;
      reachable = grown;
    }
  } else {
    const code closure = core::saturate(mgr_, top_, summands);
    reachable = diagrams.apply_local(closure, reachable);
  }
  reach_seconds_ += sw.seconds();
  return reachable;
}

void translator::do_reach(const datum& form) {
  if (top_ == core::none) fail(form, "reach before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  bool naive = false;
  std::optional<code> system;
  std::optional<code> from;
  for (std::size_t i = 2; i < form.items().size(); ++i) {
    const datum& d = form.items()[i];
    if (d.is_atom() && d.text() == "naive") naive = true;
    else if (d.is_atom() && d.text() == "saturate") continue;
    else if (d.is_atom() && d.text() == "from") {
      if (from || i + 1 >= form.items().size()) {
        fail(d, "'from' takes one bound result");
      }
      from = named(form.items()[++i]);
    } else if (!system) system = read_evterm(d);
    else fail(d, "reach takes one event term at most");
  }
  results_[name] = run_reach(naive, system, from);
}

/// `(apply NAME EVTERM SOURCE)`: the one-step image of a bound result —
/// the event applied once, no closure.
void translator::do_apply(const datum& form) {
  if (top_ == core::none) fail(form, "apply before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  const code term = read_evterm(arg(form, 2, "event term"));
  const code src = named(arg(form, 3, "source result"));
  results_[name] = mgr_.diagrams().apply_local(term, src);
}

/// `(word NAME (LEAF VAL)*)`: bind one state as a result — unlisted
/// leaves at their declared LO (0 unbounded). The syntax `get-witness`
/// prints, so exhibited states round-trip.
void translator::do_word(const datum& form) {
  if (top_ == core::none) fail(form, "word before shape");
  const std::string& name = sym(arg(form, 1, "result name"));
  std::vector<std::int32_t> values = defaults_;
  for (std::size_t i = 2; i < form.items().size(); ++i) {
    const datum& pair = form.items()[i];
    if (!pair.is_list() || pair.items().size() != 2) {
      fail(pair, "word entry must be (leaf value)");
    }
    const leaf_decl& d = require_leaf(pair.items()[0]);
    values[d.index] = as_int(pair.items()[1]);
  }
  std::size_t next = 0;
  results_[name] = build_point(top_, next, values);
}

/// Walk one word of \p c (first arc, first element throughout).
bool translator::first_word(core::shape_code sort, code c,
                std::vector<std::int32_t>& out) {
  if (c == core::none) return false;
  switch (mgr_.shapes().kind(sort)) {
    case core::shape_kind::unit:
      return true;
    case core::shape_kind::leaf:
      out.push_back(theory_->elements(c).front());
      return true;
    case core::shape_kind::pair: {
      const core::arc& a = mgr_.diagrams().arcs(c).front();
      return first_word(mgr_.shapes().head(sort), a.prime, out) &&
             first_word(mgr_.shapes().tail(sort), a.sub, out);
    }
  }
  return false;
}

/// Print \p values as a word literal: `((x 1) (y 0) …)` — valid as the
/// body of `(word …)` or a pair `(init …)`, so states round-trip.
void translator::print_word(const std::vector<std::int32_t>& values) {
  out_ << '(';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out_ << ' ';
    out_ << '(' << order_[i] << ' ' << values[i] << ')';
  }
  out_ << ')';
}

/// `(get-witness NAME)`: one state of a bound result, in word syntax —
/// the SMT get-model analogue (nonempty is sat, this is its model).
void translator::do_get_witness(const datum& form) {
  const code c = named(arg(form, 1, "result name"));
  out_ << sym(form.items()[1]) << " witness ";
  std::vector<std::int32_t> values;
  if (first_word(top_, c, values)) print_word(values);
  else out_ << "none";  // unsat: no state to exhibit
  out_ << '\n';
}

/// DFS up to \p limit complete words of \p c, emitting each.
void translator::enum_words(core::shape_code sort, code c,
                std::vector<std::int32_t>& acc, std::size_t& left,
                const std::function<void()>& k) {
  if (c == core::none || left == 0) return;
  switch (mgr_.shapes().kind(sort)) {
    case core::shape_kind::unit:
      k();
      return;
    case core::shape_kind::leaf:
      for (const std::int32_t v : theory_->elements(c)) {
        if (left == 0) return;
        acc.push_back(v);
        k();
        acc.pop_back();
      }
      return;
    case core::shape_kind::pair:
      for (const core::arc& a : mgr_.diagrams().arcs(c)) {
        if (left == 0) return;
        enum_words(mgr_.shapes().head(sort), a.prime, acc, left, [&] {
          enum_words(mgr_.shapes().tail(sort), a.sub, acc, left, k);
        });
      }
      return;
  }
}

/// `(get-states NAME [K])`: up to K states (default 10), one word
/// literal per line, after a header with the exact cardinal.
void translator::do_get_states(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  std::size_t limit = 10;
  if (form.items().size() > 2) {
    limit = static_cast<std::size_t>(as_int(form.items()[2]));
  }
  const code c = named(arg(form, 1, "result name"));
  out_ << name << ' ' << std::fixed << std::setprecision(0)
       << mgr_.diagrams().cardinal(c) << " states, showing up to " << limit
       << '\n';
  std::vector<std::int32_t> acc;
  std::size_t left = limit;
  enum_words(top_, c, acc, left, [&] {
    print_word(acc);
    out_ << '\n';
    --left;
  });
}

/// The largest value any leaf holds across the diagram \p c, by a
/// sort-directed walk: a leaf's code is a theory set to read, a pair's arcs
/// are recursed head then tail. A general statistic (also MAX_TOKEN_IN_PLACE),
/// not a query specialised to one-safety.
void translator::collect_max(code c, core::shape_code s, std::int32_t& mx,
                 std::unordered_set<code>& seen) {
  switch (mgr_.shapes().kind(s)) {
    case core::shape_kind::unit:
      return;
    case core::shape_kind::leaf:
      for (std::int32_t v : theory_->elements(c)) mx = std::max(mx, v);
      return;
    case core::shape_kind::pair:
      if (c == core::none || !seen.insert(c).second) return;
      for (const core::arc& a : mgr_.diagrams().arcs(c)) {
        collect_max(a.prime, mgr_.shapes().head(s), mx, seen);
        collect_max(a.sub, mgr_.shapes().tail(s), mx, seen);
      }
      return;
  }
}

std::int32_t translator::max_leaf_value(code c) {
  std::int32_t mx = 0;
  std::unordered_set<code> seen;
  collect_max(c, top_, mx, seen);
  return mx;
}

/// `(states [RESULT])`: the cardinal, MCC-format. With a bound result
/// (`(reach x SYSTEM)` then `(states x)`) it reads that; without one it
/// runs the default system's reach — the MCC StateSpace examination.
void translator::do_states(const datum& form) {
  if (top_ == core::none) fail(form, "states before shape");
  try {
    const code r = form.items().size() > 1 ? named(form.items()[1])
                                           : run_reach(false);
    out_ << "STATE_SPACE STATES "
         << (weighted() ? weighted_count(r) : mgr_.diagrams().cardinal_exact(r))
         << TECHNIQUES;
  } catch (const hsc::overflow_error& e) {
    out_ << "STATE_SPACE STATES CANNOT_COMPUTE\n";
    std::cerr << "overflow: " << e.what() << '\n';
    ++failures_;
  }
}

/// `(max-value NAME)`: the largest value any leaf holds in the bound
/// result — a general statistic (MAX_TOKEN_IN_PLACE; 1-safety is
/// `max-value <= 1`, judged by whoever asked).
void translator::do_max_value(const datum& form) {
  const code c = named(arg(form, 1, "result name"));
  out_ << sym(form.items()[1]) << " max-value " << max_leaf_value(c)
       << '\n';
}

/// The comparator of a query atom, by its surface spelling.
std::optional<cmp> translator::comparator(const std::string& op) {
  if (op == "<") return cmp::lt;
  if (op == "<=") return cmp::le;
  if (op == "==") return cmp::eq;
  if (op == "!=") return cmp::ne;
  if (op == ">=") return cmp::ge;
  if (op == ">") return cmp::gt;
  return std::nullopt;
}

/// One query atom applied to \p src. Two fast paths keep their dedicated
/// resolution: a leaf against a constant (or `in`) is separable, a
/// symbolic per-position meet (`select_where`); a leaf against a second
/// leaf is the crossing comparison (`select_compare`). Any other BEXP —
/// conjunction, disjunction, negation, arithmetic — compiles exactly as
/// an event guard would, a `(when ATOM)` filter applied once: separable
/// pieces fuse per leaf, crossing pieces become case brackets.
code translator::apply_atom(const datum& atom, code src) {
  if (!atom.is_list() || atom.items().empty()) {
    fail(atom, "a query atom is a boolean form over the leaves");
  }
  if (atom.items().size() >= 3 && atom.items()[1].is_atom() &&
      leaves_.contains(atom.items()[1].text())) {
    const leaf_decl& x = leaves_.at(atom.items()[1].text());
    const datum& rhs = atom.items()[2];
    const std::optional<cmp> op = comparator(atom.head());
    if (op && atom.items().size() == 3 && rhs.is_atom() &&
        leaves_.contains(rhs.text())) {
      const leaf_decl& y = leaves_.at(rhs.text());
      return hsc::select_compare(mgr_, *theory_, top_, src, x.index, *op,
                                 y.index);
    }
    if ((op && atom.items().size() == 3 && is_integer(rhs)) ||
        atom.head() == "in") {
      return hsc::select_where(mgr_, *theory_, top_, src, x.index,
                               atom_guard(atom));
    }
  }
  datum filter =
      datum::list({atom_datum("when", atom), atom}, atom.line());
  return mgr_.diagrams().apply_local(read_evterm(filter), src);
}

/// `(select NAME SOURCE ATOM+)`: filter a stored result by a conjunction of
/// query atoms and store the subset under NAME.
void translator::do_select(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  code cur = named(arg(form, 2, "source result"));
  if (form.items().size() < 4) fail(form, "select needs at least one atom");
  for (std::size_t i = 3; i < form.items().size(); ++i) {
    cur = apply_atom(form.items()[i], cur);
  }
  results_[name] = cur;
}

/// `(count NAME [exact])`: the cardinal, a double unless `exact` asks for
/// the integer itself.
void translator::do_count(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  const code c = named(arg(form, 1, "result name"));
  const bool exact = form.items().size() > 2 &&
                     sym(arg(form, 2, "count modifier")) == "exact";
  if (form.items().size() > 2 && !exact) {
    fail(form.items()[2], "count accepts the single modifier `exact`");
  }
  out_ << name << " count ";
  if (exact) {
    out_ << (weighted() ? weighted_count(c) : mgr_.diagrams().cardinal_exact(c));
  } else {
    out_ << std::fixed << std::setprecision(0) << mgr_.diagrams().cardinal(c);
  }
  out_ << '\n';
}

void translator::do_nodes(const datum& form) {
  const code c = named(arg(form, 1, "result name"));
  out_ << sym(form.items()[1]) << " nodes " << mgr_.diagrams().size(c)
       << '\n';
}

/// `(profile NAME)`: the representation size level by level — for every sort
/// of the shape reached by the result, in frontier order, the leaves it spans,
/// its distinct nodes and their arcs. A wide belly between two levels says
/// those levels are correlated: bring them closer, or bracket what lies
/// between them.
void translator::do_profile(const datum& form) {
  const code c = named(arg(form, 1, "result name"));
  const core::shape_table& sh = mgr_.shapes();
  // The span of every sort of the shape: first frontier position and width.
  std::unordered_map<core::shape_code, std::pair<std::size_t, std::size_t>> span;
  std::size_t next = 0;
  const auto walk = [&](auto&& self, core::shape_code s) -> std::pair<std::size_t, std::size_t> {
    if (sh.kind(s) == core::shape_kind::leaf) {
      const auto r = std::make_pair(next++, std::size_t{1});
      span[s] = r;
      return r;
    }
    if (sh.kind(s) == core::shape_kind::unit) return {next, 0};
    const auto h = self(self, sh.head(s));
    const auto t = self(self, sh.tail(s));
    const auto r = std::make_pair(h.first, h.second + t.second);
    span[s] = r;
    return r;
  };
  walk(walk, top_);
  // Distinct nodes and arcs per sort.
  std::unordered_map<core::shape_code, std::pair<std::size_t, std::size_t>> counts;
  std::unordered_set<code> seen;
  std::vector<core::shape_code> order;
  const auto visit = [&](auto&& self, code n) -> void {
    if (n == core::none || !seen.insert(n).second) return;
    const core::shape_code s = mgr_.diagrams().sort_of(n);
    if (sh.kind(s) != core::shape_kind::pair) return;
    auto& k = counts[s];
    if (k.first == 0) order.push_back(s);
    ++k.first;
    const std::span<const core::arc> arcs = mgr_.diagrams().arcs(n);
    k.second += arcs.size();
    const bool head_node = sh.kind(sh.head(s)) == core::shape_kind::pair;
    const bool tail_node = sh.kind(sh.tail(s)) == core::shape_kind::pair;
    for (const core::arc& a : arcs) {
      if (head_node) self(self, a.prime);
      if (tail_node) self(self, a.sub);
    }
  };
  visit(visit, c);
  std::ranges::sort(order, [&](core::shape_code a, core::shape_code b) {
    const auto [fa, wa] = span[a];
    const auto [fb, wb] = span[b];
    return fa != fb ? fa < fb : wa > wb;
  });
  out_ << sym(form.items()[1]) << " profile " << order.size() << " levels\n";
  for (const core::shape_code s : order) {
    const auto [first, width] = span[s];
    const auto [nodes, arcs] = counts[s];
    out_ << "  " << order_[first];
    if (width > 1) out_ << ".." << order_[first + width - 1] << " (" << width << ')';
    out_ << " nodes " << nodes << " arcs " << arcs << '\n';
  }
}

void translator::do_print(const datum& form) {
  const code c = named(arg(form, 1, "result name"));
  out_ << sym(form.items()[1]) << " = ";
  mgr_.diagrams().print(out_, c);
  out_ << '\n';
}

/// `(expect NAME N)`: N within 32 bits is compared to the double cardinal;
/// a larger literal is compared exactly.
void translator::do_expect(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  const code c = named(arg(form, 1, "result name"));
  const datum& lit = arg(form, 2, "count");
  if (is_integer(lit) && !weighted()) {
    const double want = static_cast<double>(as_int(lit));
    const double got = mgr_.diagrams().cardinal(c);
    if (got == want) {
      out_ << "ok " << name << " == " << want << '\n';
    } else {
      out_ << "FAIL " << name << " expected " << want << " got " << got
           << '\n';
      ++failures_;
    }
    return;
  }
  if (!lit.is_atom() || lit.text().empty() ||
      lit.text().find_first_not_of("0123456789") != std::string::npos) {
    fail(lit, "expected a non-negative integer literal");
  }
  const mpz_class want(lit.text());
  const mpz_class got =
      weighted() ? weighted_count(c) : mgr_.diagrams().cardinal_exact(c);
  if (got == want) {
    out_ << "ok " << name << " == " << want << '\n';
  } else {
    out_ << "FAIL " << name << " expected " << want << " got " << got << '\n';
    ++failures_;
  }
}

void translator::do_bill(const datum&) {
  out_ << "bill: " << mgr_.diagrams().node_count() << " nodes, "
       << mgr_.operations().size() << " op terms, " << reach_seconds_
       << " s in reach\n";
}

}  // namespace hsc::surface
