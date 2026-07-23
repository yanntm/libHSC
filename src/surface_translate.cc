/// \file surface_translate.cc
/// \brief M2M: `datum` forms → operations on a `core::manager`.
///
/// The pass reads the surface's forms into engine calls: leaves become a shared
/// int leaf sort with per-name bounds, the shape is interned, each event is
/// compiled to one `core::product` term over the separable Presburger fragment
/// (§6), and commands run in order. A crossing atom or action (one that would
/// need `split_equiv`, §7) is refused by name rather than mis-compiled.

#include "hsc/surface/translate.hh"

#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/query.hh"
#include "hsc/util/errors.hh"
#include "hsc/util/timing.hh"

namespace hsc::surface {

namespace {

using core::code;

/// The trailing clause every MCC answer line carries (and the newline). Our
/// verdicts come from saturated decision diagrams.
constexpr const char* TECHNIQUES = " TECHNIQUES DECISION_DIAGRAMS SATURATION\n";

/// A declared leaf: its frontier position, and an optional bound. The type
/// is Int; `(leaf NAME LO HI)` opts into a finite domain a compiler may
/// exploit (and an init default of LO), it is never required.
struct leaf_decl {
  bool bounded = false;
  std::int32_t lo = 0;
  std::int32_t hi = 0;
  std::size_t index = 0;  ///< frontier position, assigned when the shape is read
  bool placed = false;    ///< set once the shape uses it (exactly once)
};

/// One leaf's contribution to an event: a symbolic guard (a `bexpr` over the
/// coordinate) and at most one action.
struct leaf_effect {
  bool has_guard = false;
  lia::bexpr guard = lia::btrue;  ///< conjunction of the leaf's atoms
  enum class act { none, assign, shift } action = act::none;
  std::int32_t arg = 0;
};

class translator {
 public:
  explicit translator(std::ostream& out) : out_(out) {
    auto [index, theory] = mgr_.import<leaves::int_set_theory>();
    theory_index_ = index;
    theory_ = &theory;
    leaf_sort_ = mgr_.shapes().leaf(index);
  }

  int run(const std::vector<datum>& forms) {
    for (const datum& form : forms) dispatch(form);
    return failures_;
  }

 private:
  // --- small syntax helpers ------------------------------------------------

  [[noreturn]] static void fail(const datum& d, const std::string& msg) {
    throw translate_error(d.line(), msg);
  }

  static const std::string& sym(const datum& d) {
    if (!d.is_atom()) fail(d, "expected a symbol, found a list");
    return d.text();
  }

  static std::int32_t as_int(const datum& d) {
    if (!d.is_atom()) fail(d, "expected an integer, found a list");
    const std::string& t = d.text();
    std::int32_t v = 0;
    const auto* end = t.data() + t.size();
    const auto res = std::from_chars(t.data(), end, v);
    if (res.ec != std::errc{} || res.ptr != end) {
      fail(d, "expected an integer, found '" + t + "'");
    }
    return v;
  }

  const datum& arg(const datum& form, std::size_t i, const char* what) {
    if (i >= form.items().size()) fail(form, std::string("missing ") + what);
    return form.items()[i];
  }

  /// The value side of an atom or action must be a constant. Anything else —
  /// a list `(+ b c)` or a bare leaf name — relates a second coordinate and so
  /// needs `split_equiv` (§7): parsed and understood, then declined.
  std::int32_t require_constant(const datum& d, const char* role) {
    if (d.is_atom()) {
      const std::string& t = d.text();
      std::int32_t v = 0;
      const auto* end = t.data() + t.size();
      const auto res = std::from_chars(t.data(), end, v);
      if (res.ec == std::errc{} && res.ptr == end) return v;
    }
    fail(d, std::string("crossing ") + role +
                ": value is not a constant; relating two leaves needs "
                "split_equiv (§7), not implemented");
  }

  // --- form dispatch -------------------------------------------------------

  void dispatch(const datum& form) {
    if (!form.is_list() || form.items().empty()) {
      fail(form, "a top-level form must be a non-empty list");
    }
    const std::string& kw = form.head();
    if (kw == "leaf") do_leaf(form);
    else if (kw == "shape") do_shape(form);
    else if (kw == "init") do_init(form);
    else if (kw == "event") do_event(form);
    else if (kw == "reach") do_reach(form);
    else if (kw == "states") do_states(form);
    else if (kw == "one-safe") do_one_safe(form);
    else if (kw == "deadlock") do_deadlock(form);
    else if (kw == "select") do_select(form);
    else if (kw == "count") do_count(form);
    else if (kw == "nodes") do_nodes(form);
    else if (kw == "print") do_print(form);
    else if (kw == "expect") do_expect(form);
    else if (kw == "bill") do_bill(form);
    else fail(form, "unknown form '" + kw + "'");
  }

  void do_leaf(const datum& form) {
    const std::string& name = sym(arg(form, 1, "leaf name"));
    if (leaves_.contains(name)) fail(form, "leaf '" + name + "' redeclared");
    leaf_decl d;
    if (form.items().size() > 2) {  // optional: (leaf NAME LO HI)
      d.bounded = true;
      d.lo = as_int(arg(form, 2, "lower bound"));
      d.hi = as_int(arg(form, 3, "upper bound"));
      if (d.hi <= d.lo) fail(form, "empty domain [" + std::to_string(d.lo) +
                                       "," + std::to_string(d.hi) + ")");
    }
    leaves_.emplace(name, d);
  }

  // --- the shape, and the frontier order it fixes --------------------------

  core::shape_code build_sort(const datum& d) {
    if (d.is_atom()) {
      const std::string& name = d.text();
      if (name == "unit") return mgr_.shapes().unit();
      auto it = leaves_.find(name);
      if (it == leaves_.end()) fail(d, "shape uses undeclared leaf '" + name + "'");
      if (it->second.placed) fail(d, "leaf '" + name + "' used twice in the shape");
      it->second.placed = true;
      it->second.index = order_.size();
      order_.push_back(name);
      return leaf_sort_;
    }
    const std::string& ctor = d.head();
    const auto& items = d.items();
    if (ctor == "pair") {
      if (items.size() != 3) fail(d, "pair takes exactly two sorts");
      const core::shape_code h = build_sort(items[1]);
      const core::shape_code t = build_sort(items[2]);
      return mgr_.shapes().pair(h, t);
    }
    if (ctor == "spine") return build_spine(d, 1);
    if (ctor == "balanced") {
      if (items.size() < 2) fail(d, "balanced needs at least one sort");
      return build_balanced(std::span(items).subspan(1));
    }
    fail(d, "unknown sort constructor '" + ctor + "'");
  }

  /// `(spine a b …)` → pair(a, pair(b, … unit)). Frontier is a, b, ….
  core::shape_code build_spine(const datum& d, std::size_t from) {
    if (from >= d.items().size()) return mgr_.shapes().unit();
    const core::shape_code head = build_sort(d.items()[from]);
    return mgr_.shapes().pair(head, build_spine(d, from + 1));
  }

  /// Split in half, left-biased. Frontier is the list order.
  core::shape_code build_balanced(std::span<const datum> xs) {
    if (xs.size() == 1) return build_sort(xs[0]);
    const std::size_t mid = (xs.size() + 1) / 2;
    const core::shape_code h = build_balanced(xs.subspan(0, mid));
    const core::shape_code t = build_balanced(xs.subspan(mid));
    return mgr_.shapes().pair(h, t);
  }

  void do_shape(const datum& form) {
    if (top_ != core::none) fail(form, "shape declared twice");
    order_.clear();
    top_ = build_sort(arg(form, 1, "sort"));
    for (const auto& [name, decl] : leaves_) {
      if (!decl.placed) fail(form, "leaf '" + name + "' is not in the shape");
    }
    init_.assign(order_.size(), 0);
    for (std::size_t i = 0; i < order_.size(); ++i) {
      const leaf_decl& d = leaves_.at(order_[i]);
      if (d.bounded) init_[i] = d.lo;  // bounded default: the low end; else 0
    }
  }

  const leaf_decl& require_leaf(const datum& d) {
    const std::string& name = sym(d);
    auto it = leaves_.find(name);
    if (it == leaves_.end()) fail(d, "unknown leaf '" + name + "'");
    if (top_ == core::none) fail(d, "leaf used before the shape is declared");
    return it->second;
  }

  // --- the initial word ----------------------------------------------------

  void do_init(const datum& form) {
    if (top_ == core::none) fail(form, "init before shape");
    for (std::size_t i = 1; i < form.items().size(); ++i) {
      const datum& pair = form.items()[i];
      if (!pair.is_list() || pair.items().size() != 2) {
        fail(pair, "init entry must be (leaf value)");
      }
      const leaf_decl& d = require_leaf(pair.items()[0]);
      init_[d.index] = as_int(pair.items()[1]);
    }
    init_set_ = true;
  }

  code build_point(core::shape_code sort, std::size_t& next) {
    core::diagram_engine& diagrams = mgr_.diagrams();
    switch (mgr_.shapes().kind(sort)) {
      case core::shape_kind::unit:
        return diagrams.one();
      case core::shape_kind::leaf:
        return theory_->singleton(init_[next++]);
      case core::shape_kind::pair: {
        const code head = build_point(mgr_.shapes().head(sort), next);
        const code tail = build_point(mgr_.shapes().tail(sort), next);
        return diagrams.rectangle(sort, head, tail);
      }
    }
    return core::none;
  }

  code initial() {
    std::size_t next = 0;
    return build_point(top_, next);
  }

  // --- events: the separable Presburger fragment ---------------------------

  /// The predicate a single atom `(cmp leaf K)` or `(in leaf K…)` puts on
  /// the leaf's coordinate (`lia` position 0) — symbolic, no domain
  /// materialized. Refuses anything relating a second leaf (§7).
  lia::bexpr atom_guard(const datum& atom) {
    lia::expr_factory& ex = theory_->exprs();
    const lia::iexpr v0 = ex.variable(0);
    const std::string& op = atom.head();
    const auto& items = atom.items();
    if (op == "in") {  // an enumeration by nature: a disjunction of ==
      std::vector<lia::bexpr> alts;
      for (std::size_t i = 2; i < items.size(); ++i) {
        alts.push_back(
            ex.compare(lia::bkind::eq, v0, ex.constant(as_int(items[i]))));
      }
      return ex.disj(alts);
    }
    if (items.size() != 3) fail(atom, "comparison takes a leaf and a constant");
    const lia::iexpr k = ex.constant(require_constant(items[2], "constraint"));
    if (op == "==") return ex.compare(lia::bkind::eq, v0, k);
    if (op == "!=") return ex.compare(lia::bkind::neq, v0, k);
    if (op == "<") return ex.compare(lia::bkind::lt, v0, k);
    if (op == "<=") return ex.compare(lia::bkind::leq, v0, k);
    if (op == ">") return ex.compare(lia::bkind::gt, v0, k);
    if (op == ">=") return ex.compare(lia::bkind::geq, v0, k);
    fail(atom, "unknown comparison '" + op + "'");
  }

  /// Accumulate an atom into the per-leaf effect map.
  void take_atom(const datum& atom,
                 std::unordered_map<std::string, leaf_effect>& eff) {
    if (!atom.is_list() || atom.items().size() < 2) {
      fail(atom, "an atom is (cmp leaf constant)");
    }
    const datum& leaf_ref = atom.items()[1];
    require_leaf(leaf_ref);
    const lia::bexpr g = atom_guard(atom);
    leaf_effect& e = eff[sym(leaf_ref)];
    e.guard = e.has_guard ? theory_->exprs().conj(e.guard, g) : g;
    e.has_guard = true;
  }

  /// Accumulate an action into the per-leaf effect map.
  void take_action(const datum& act,
                   std::unordered_map<std::string, leaf_effect>& eff) {
    const std::string& op = act.head();
    if (act.items().size() != 3) fail(act, "an action is (op leaf constant)");
    const leaf_decl& d = require_leaf(act.items()[1]);
    (void)d;
    const std::int32_t k = require_constant(act.items()[2], "assignment");
    leaf_effect& e = eff[sym(act.items()[1])];
    if (e.action != leaf_effect::act::none) {
      fail(act, "leaf '" + sym(act.items()[1]) +
                    "' assigned twice in one event; pre-fold in the generator");
    }
    if (op == ":=") { e.action = leaf_effect::act::assign; e.arg = k; }
    else if (op == "+=") { e.action = leaf_effect::act::shift; e.arg = k; }
    else if (op == "-=") { e.action = leaf_effect::act::shift; e.arg = -k; }
    else fail(act, "unknown action '" + op + "'");
  }

  void do_event(const datum& form) {
    if (top_ == core::none) fail(form, "event before shape");
    std::unordered_map<std::string, leaf_effect> eff;
    for (std::size_t i = 2; i < form.items().size(); ++i) {
      const datum& clause = form.items()[i];
      const std::string& kw = clause.head();
      if (kw == "when") {
        for (std::size_t a = 1; a < clause.items().size(); ++a) {
          take_atom(clause.items()[a], eff);
        }
      } else if (kw == "do") {
        for (std::size_t a = 1; a < clause.items().size(); ++a) {
          take_action(clause.items()[a], eff);
        }
      } else {
        fail(clause, "an event clause is (when …) or (do …)");
      }
    }

    // Assemble the by-leaf terms. A guard that folded to false makes the
    // whole event dead, so it is never built; a merely unsatisfiable-in-
    // practice guard is an honest never-firing term. The guard expressions
    // are kept beside the event so deadlock can re-filter by them.
    std::vector<code> by_leaf(order_.size(), core::op_table::id);
    std::vector<std::pair<std::size_t, lia::bexpr>> guards;
    for (const auto& [name, e] : eff) {
      if (e.has_guard && e.guard == lia::bfalse) return;  // dead event
      const std::size_t idx = leaves_.at(name).index;
      const lia::bexpr guard = e.has_guard ? e.guard : lia::btrue;
      if (e.has_guard) guards.emplace_back(idx, e.guard);
      switch (e.action) {
        case leaf_effect::act::assign:
          by_leaf[idx] = theory_->assign_if(guard, e.arg);
          break;
        case leaf_effect::act::shift:
          by_leaf[idx] = theory_->shift_if(guard, e.arg);
          break;
        case leaf_effect::act::none:
          by_leaf[idx] = theory_->keep_if(guard);
          break;
      }
    }
    events_.push_back(
        core::product(mgr_.operations(), mgr_.shapes(), top_, by_leaf));
    guards_.push_back(std::move(guards));
  }

  // --- commands ------------------------------------------------------------

  code named(const datum& d) {
    const std::string& name = sym(d);
    auto it = results_.find(name);
    if (it == results_.end()) fail(d, "no result named '" + name + "'");
    return it->second;
  }

  /// The least fixpoint from the initial word. `naive` iterates, `saturate`
  /// applies the static closure; both denote the same diagram. Propagates
  /// `hsc::overflow_error` if a leaf value leaves its representable range.
  code run_reach(bool naive) {
    core::diagram_engine& diagrams = mgr_.diagrams();
    code reachable = initial();
    util::stopwatch sw;
    if (naive) {
      const code all = mgr_.operations().sum(events_);
      for (;;) {
        const code grown =
            diagrams.join(reachable, diagrams.apply_local(all, reachable));
        if (grown == reachable) break;
        reachable = grown;
      }
    } else {
      const code closure = core::saturate(mgr_, top_, events_);
      reachable = diagrams.apply_local(closure, reachable);
    }
    reach_seconds_ += sw.seconds();
    return reachable;
  }

  void do_reach(const datum& form) {
    if (top_ == core::none) fail(form, "reach before shape");
    const std::string& name = sym(arg(form, 1, "result name"));
    bool naive = false;
    if (form.items().size() > 2) {
      const std::string& how = sym(form.items()[2]);
      if (how == "naive") naive = true;
      else if (how != "saturate") fail(form, "strategy is saturate or naive");
    }
    results_[name] = run_reach(naive);
  }

  /// The largest value any leaf holds across the diagram \p c, by a
  /// sort-directed walk: a leaf's code is a theory set to read, a pair's arcs
  /// are recursed head then tail. A general statistic (also MAX_TOKEN_IN_PLACE),
  /// not a query specialised to one-safety.
  void collect_max(code c, core::shape_code s, std::int32_t& mx,
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

  [[nodiscard]] std::int32_t max_leaf_value(code c) {
    std::int32_t mx = 0;
    std::unordered_set<code> seen;
    collect_max(c, top_, mx, seen);
    return mx;
  }

  /// The states of \p from where an event's guards all hold: successive
  /// per-position filters by each guard expression. No domain cylinder — only
  /// values the diagram actually holds are consulted.
  code enabled_in(code from,
                  const std::vector<std::pair<std::size_t, lia::bexpr>>& gs) {
    code cur = from;
    for (const auto& [pos, g] : gs) {
      if (cur == core::none) break;
      cur = hsc::select_where(mgr_, *theory_, top_, cur, pos, g);
    }
    return cur;
  }

  void do_states(const datum& form) {
    if (top_ == core::none) fail(form, "states before shape");
    try {
      const code r = run_reach(false);
      out_ << "STATE_SPACE STATES " << std::fixed << std::setprecision(0)
           << mgr_.diagrams().cardinal(r) << TECHNIQUES;
    } catch (const hsc::overflow_error& e) {
      out_ << "STATE_SPACE STATES CANNOT_COMPUTE\n";
      std::cerr << "overflow: " << e.what() << '\n';
      ++failures_;
    }
  }

  void do_one_safe(const datum& form) {
    if (top_ == core::none) fail(form, "one-safe before shape");
    code r;
    try {
      r = run_reach(false);
    } catch (const hsc::overflow_error&) {
      out_ << "FORMULA OneSafe FALSE" << TECHNIQUES;  // unbounded: a place > 1
      return;
    }
    out_ << "FORMULA OneSafe " << (max_leaf_value(r) <= 1 ? "TRUE" : "FALSE")
         << TECHNIQUES;
  }

  void do_deadlock(const datum& form) {
    if (top_ == core::none) fail(form, "deadlock before shape");
    code r;
    try {
      r = run_reach(false);
    } catch (const hsc::overflow_error& e) {
      out_ << "DEADLOCK CANNOT_COMPUTE\n";
      std::cerr << "overflow: " << e.what() << '\n';
      ++failures_;
      return;
    }
    core::diagram_engine& diagrams = mgr_.diagrams();
    code enabled = core::none;  // union over events of "guards hold in R"
    for (const auto& guards : guards_) {
      const code en = enabled_in(r, guards);
      if (en == core::none) continue;
      enabled = enabled == core::none ? en : diagrams.join(enabled, en);
    }
    const code dead = enabled == core::none ? r : diagrams.minus(r, enabled);
    out_ << "FORMULA ReachabilityDeadlock "
         << (dead == core::none ? "FALSE" : "TRUE") << TECHNIQUES;
  }

  /// The comparator of a query atom, by its surface spelling.
  static std::optional<cmp> comparator(const std::string& op) {
    if (op == "<") return cmp::lt;
    if (op == "<=") return cmp::le;
    if (op == "==") return cmp::eq;
    if (op == "!=") return cmp::ne;
    if (op == ">=") return cmp::ge;
    if (op == ">") return cmp::gt;
    return std::nullopt;
  }

  /// One query atom applied to \p src. A right-hand side naming a second leaf
  /// is the crossing comparison, resolved by the §7 case (`select_compare`);
  /// a constant right-hand side (or `in`) is separable, a symbolic
  /// per-position filter.
  code apply_atom(const datum& atom, code src) {
    if (!atom.is_list() || atom.items().size() < 3) {
      fail(atom, "a query atom is (cmp leaf constant-or-leaf) or (in leaf K+)");
    }
    const leaf_decl& x = require_leaf(atom.items()[1]);
    const datum& rhs = atom.items()[2];
    if (atom.items().size() == 3 && rhs.is_atom() &&
        leaves_.contains(rhs.text())) {
      const std::optional<cmp> op = comparator(atom.head());
      if (!op) fail(atom, "unknown comparison '" + atom.head() + "'");
      const leaf_decl& y = leaves_.at(rhs.text());
      return hsc::select_compare(mgr_, *theory_, top_, src, x.index, *op,
                                 y.index);
    }
    return hsc::select_where(mgr_, *theory_, top_, src, x.index,
                             atom_guard(atom));
  }

  /// `(select NAME SOURCE ATOM+)`: filter a stored result by a conjunction of
  /// query atoms and store the subset under NAME.
  void do_select(const datum& form) {
    const std::string& name = sym(arg(form, 1, "result name"));
    code cur = named(arg(form, 2, "source result"));
    if (form.items().size() < 4) fail(form, "select needs at least one atom");
    for (std::size_t i = 3; i < form.items().size(); ++i) {
      cur = apply_atom(form.items()[i], cur);
    }
    results_[name] = cur;
  }

  void do_count(const datum& form) {
    const code c = named(arg(form, 1, "result name"));
    out_ << sym(form.items()[1]) << " count " << std::fixed
         << std::setprecision(0) << mgr_.diagrams().cardinal(c) << '\n';
  }

  void do_nodes(const datum& form) {
    const code c = named(arg(form, 1, "result name"));
    out_ << sym(form.items()[1]) << " nodes " << mgr_.diagrams().size(c) << '\n';
  }

  void do_print(const datum& form) {
    const code c = named(arg(form, 1, "result name"));
    out_ << sym(form.items()[1]) << " = ";
    mgr_.diagrams().print(out_, c);
    out_ << '\n';
  }

  void do_expect(const datum& form) {
    const code c = named(arg(form, 1, "result name"));
    const double want = static_cast<double>(as_int(arg(form, 2, "count")));
    const double got = mgr_.diagrams().cardinal(c);
    const std::string& name = sym(form.items()[1]);
    if (got == want) {
      out_ << "ok " << name << " == " << want << '\n';
    } else {
      out_ << "FAIL " << name << " expected " << want << " got " << got << '\n';
      ++failures_;
    }
  }

  void do_bill(const datum&) {
    out_ << "bill: " << mgr_.diagrams().node_count() << " nodes, "
         << mgr_.operations().size() << " op terms, " << reach_seconds_
         << " s in reach\n";
  }

  // --- state ---------------------------------------------------------------

  std::ostream& out_;
  core::manager mgr_;
  core::theory_index theory_index_ = 0;
  leaves::int_set_theory* theory_ = nullptr;
  core::shape_code leaf_sort_ = core::none;

  std::unordered_map<std::string, leaf_decl> leaves_;
  std::vector<std::string> order_;  ///< leaf names in frontier order
  core::shape_code top_ = core::none;
  std::vector<std::int32_t> init_;
  bool init_set_ = false;

  std::vector<code> events_;
  /// Per event, its guard expression at each guarded frontier position.
  std::vector<std::vector<std::pair<std::size_t, lia::bexpr>>> guards_;
  std::unordered_map<std::string, code> results_;
  double reach_seconds_ = 0.0;
  int failures_ = 0;
};

}  // namespace

int translate(const std::vector<datum>& forms, std::ostream& out) {
  translator t(out);
  return t.run(forms);
}

int run_file(const std::string& path, std::ostream& out, std::ostream& err) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    err << "cannot open " << path << '\n';
    return 2;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  const std::string text = buf.str();
  try {
    const std::vector<datum> forms = parse(text);
    return translate(forms, out) == 0 ? 0 : 1;
  } catch (const parse_error& e) {
    err << path << ": parse error: " << e.what() << '\n';
    return 2;
  } catch (const translate_error& e) {
    err << path << ": " << e.what() << '\n';
    return 2;
  }
}

}  // namespace hsc::surface
