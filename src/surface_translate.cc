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
#include <map>
#include <memory>
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
#include "hsc/event.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/query.hh"
#include "hsc/surface/expr.hh"
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

/// One leaf's separable contribution to an event: a symbolic guard and at
/// most one assignment, both `lia` expressions over the coordinate
/// (position 0). `apply_if` folds the assignment to assign/shift where the
/// expression is one.
struct leaf_effect {
  bool has_guard = false;
  lia::bexpr guard = lia::btrue;  ///< conjunction of the leaf's atoms
  bool has_rhs = false;
  lia::iexpr rhs0 = 0;  ///< x := rhs0(x), when present
};

/// A declared array: `(array NAME CELL…)` names its cell leaves in index
/// order. The cells may sit anywhere in the shape — placement resolves to
/// frontier positions on first use, after the shape is declared.
struct array_decl {
  std::vector<std::string> cells;
  bool resolved = false;
  std::vector<std::uint32_t> positions;
};

class translator final : public name_scope {
 public:
  explicit translator(std::ostream& out) : out_(out) {
    auto [index, theory] = mgr_.import<leaves::int_set_theory>();
    theory_index_ = index;
    theory_ = &theory;
    leaf_sort_ = mgr_.shapes().leaf(index);
    cases_ = std::make_unique<case_engine>(mgr_, theory);
    reader_ = std::make_unique<expr_reader>(theory.exprs(), *this);
  }

  /// \name name_scope: what the expression reader resolves through
  ///@{
  [[nodiscard]] std::optional<std::uint32_t> position(
      const std::string& name) const override {
    const auto it = leaves_.find(name);
    if (it == leaves_.end() || !it->second.placed) return std::nullopt;
    return static_cast<std::uint32_t>(it->second.index);
  }
  [[nodiscard]] std::optional<std::vector<std::uint32_t>> array(
      const std::string& name) const override {
    const auto it = arrays_.find(name);
    if (it == arrays_.end()) return std::nullopt;
    array_decl& a = it->second;
    if (!a.resolved) {
      a.positions.reserve(a.cells.size());
      for (const std::string& cell : a.cells) {
        const auto c = leaves_.find(cell);
        if (c == leaves_.end() || !c->second.placed) {
          throw translate_error(0, "array '" + name + "': cell '" + cell +
                                       "' is not a placed leaf");
        }
        a.positions.push_back(static_cast<std::uint32_t>(c->second.index));
      }
      a.resolved = true;
    }
    return a.positions;
  }
  ///@}

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
    else if (kw == "array") do_array(form);
    else if (kw == "alt") do_combinator(form, /*is_alt=*/true);
    else if (kw == "seq") do_combinator(form, /*is_alt=*/false);
    else if (kw == "shape") do_shape(form);
    else if (kw == "init") do_init(form);
    else if (kw == "event") do_event(form);
    else if (kw == "reach") do_reach(form);
    else if (kw == "states") do_states(form);
    else if (kw == "max-value") do_max_value(form);
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

  /// `(array NAME CELL…)`: the named leaves, in index order, are
  /// addressable as NAME[i]. Cells may sit anywhere in the shape;
  /// placement resolves on first use.
  void do_array(const datum& form) {
    const std::string& name = sym(arg(form, 1, "array name"));
    if (form.items().size() < 3) fail(form, "array needs at least one cell");
    if (arrays_.contains(name)) fail(form, "array '" + name + "' redeclared");
    array_decl a;
    for (std::size_t i = 2; i < form.items().size(); ++i) {
      const std::string& cell = sym(form.items()[i]);
      if (!leaves_.contains(cell)) {
        fail(form.items()[i], "array cell '" + cell + "' is not a leaf");
      }
      a.cells.push_back(cell);
    }
    arrays_.emplace(name, std::move(a));
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

  /// One parsed assignment: target and value as expressions over frontier
  /// positions. The target is a variable or an array access.
  struct parsed_assign {
    lia::iexpr lhs;
    lia::iexpr rhs;
  };

  [[nodiscard]] bool is_node_kind(lia::iexpr e, lia::ikind k) const {
    return (e & 1u) == 0 && e != 0 && theory_->exprs().kind(e) == k;
  }

  /// Parse one action `(op LHS EXPR)`; `+=`/`-=` desugar to `:=` with the
  /// target read on the right.
  parsed_assign parse_action(const datum& act) {
    if (!act.is_list() || act.items().size() != 3) {
      fail(act, "an action is (:= lhs expr), (+= lhs expr) or (-= lhs expr)");
    }
    const std::string& op = act.head();
    lia::expr_factory& ex = theory_->exprs();
    const lia::iexpr lhs = reader_->read_int(act.items()[1]);
    if (!is_node_kind(lhs, lia::ikind::var) &&
        !is_node_kind(lhs, lia::ikind::array) && lhs != lia::iundef) {
      fail(act.items()[1], "assignment target is not a leaf or array cell");
    }
    lia::iexpr rhs = reader_->read_int(act.items()[2]);
    if (op == "+=") rhs = ex.add(lhs, rhs);
    else if (op == "-=") rhs = ex.sub(lhs, rhs);
    else if (op != ":=") fail(act, "unknown action '" + op + "'");
    return {lhs, rhs};
  }

  /// The product term of per-position separable effects.
  code separable_product(const std::map<std::size_t, leaf_effect>& eff) {
    std::vector<code> by_leaf(order_.size(), core::op_table::id);
    for (const auto& [pos, e] : eff) {
      const lia::bexpr g = e.has_guard ? e.guard : lia::btrue;
      by_leaf[pos] =
          e.has_rhs ? theory_->apply_if(g, e.rhs0) : theory_->keep_if(g);
    }
    return core::product(mgr_.operations(), mgr_.shapes(), top_, by_leaf);
  }

  /// A dead event still names a term — the zero term, which never fires:
  /// absent from an alt, fatal to a seq.
  void dead_event(const datum& form, const std::string& name) {
    define_event(form, name, cases_->make_event(top_, lia::bfalse, {}));
  }

  void do_event(const datum& form) {
    if (top_ == core::none) fail(form, "event before shape");
    const std::string& name = sym(arg(form, 1, "event name"));
    lia::expr_factory& ex = theory_->exprs();

    // Read the guard atoms — separable single-position atoms fuse per
    // leaf, anything else is a crossing filter (a §7 case bracket, applied
    // before any action so every guard reads the pre-state) — and the do
    // clauses, kept in order as atomic sequential steps.
    std::map<std::size_t, leaf_effect> eff;
    std::vector<lia::bexpr> filters;
    std::vector<std::vector<parsed_assign>> clauses;
    for (std::size_t i = 2; i < form.items().size(); ++i) {
      const datum& clause = form.items()[i];
      const std::string& kw = clause.head();
      if (kw == "when") {
        for (std::size_t a = 1; a < clause.items().size(); ++a) {
          const lia::bexpr b = reader_->read_bool(clause.items()[a]);
          if (b == lia::bfalse || b == lia::bundef) { dead_event(form, name); return; }
          if (b == lia::btrue) continue;
          const auto supp = ex.support_bool(b);
          if (ex.array_positions_bool(b).empty() && supp.size() == 1) {
            const std::size_t p = supp[0];
            const lia::bexpr b0 =
                ex.shift_positions_bool(b, -static_cast<std::int32_t>(p));
            leaf_effect& e = eff[p];
            e.guard = e.has_guard ? ex.conj(e.guard, b0) : b0;
            e.has_guard = true;
          } else {
            filters.push_back(b);
          }
        }
      } else if (kw == "do") {
        std::vector<parsed_assign> acts;
        for (std::size_t a = 1; a < clause.items().size(); ++a) {
          acts.push_back(parse_action(clause.items()[a]));
        }
        clauses.push_back(std::move(acts));
      } else {
        fail(clause, "an event clause is (when …) or (do …)");
      }
    }
    for (const auto& [pos, e] : eff) {
      if (e.guard == lia::bfalse) { dead_event(form, name); return; }
    }

    // Compile each do clause. It is separable when every assignment writes
    // one position that at most itself is read; then it is a product of
    // independent leaf terms. Otherwise the whole clause becomes one case
    // bracket — its assignments stay simultaneous, reads pre-clause.
    struct compiled_clause {
      std::map<std::size_t, leaf_effect> sep;
      std::vector<case_engine::assign> cross;  ///< empty when separable
    };
    std::vector<compiled_clause> steps;
    for (const auto& acts : clauses) {
      compiled_clause cc;
      bool crossing = false;
      for (const parsed_assign& a : acts) {
        if (a.lhs == lia::iundef || a.rhs == lia::iundef) { dead_event(form, name); return; }
        if (!is_node_kind(a.lhs, lia::ikind::var)) {
          crossing = true;
          continue;
        }
        const auto p = static_cast<std::size_t>(ex.node(a.lhs).payload);
        const auto supp = ex.support(a.rhs);
        if (!ex.array_positions(a.rhs).empty() ||
            !(supp.empty() || (supp.size() == 1 && supp[0] == p))) {
          crossing = true;
        }
      }
      if (crossing) {
        for (const parsed_assign& a : acts) {
          cc.cross.push_back({a.lhs, a.rhs});
        }
      } else {
        for (const parsed_assign& a : acts) {
          const auto p = static_cast<std::size_t>(ex.node(a.lhs).payload);
          leaf_effect& s = cc.sep[p];
          if (s.has_rhs) {
            fail(form, "a position is assigned twice in one do clause");
          }
          s.has_rhs = true;
          s.rhs0 = ex.shift_positions(a.rhs, -static_cast<std::int32_t>(p));
        }
      }
      steps.push_back(std::move(cc));
    }

    // Assemble in application order: the crossing filters, the product
    // fusing separable guards with the first clause's separable
    // assignments, then each remaining step.
    std::vector<code> parts;
    for (const lia::bexpr b : filters) {
      parts.push_back(cases_->make_event(top_, b, {}));
    }
    std::size_t first = 0;
    if (!steps.empty() && steps.front().cross.empty()) {
      for (auto& [p, e] : steps.front().sep) {
        leaf_effect& g = eff[p];
        g.has_rhs = true;
        g.rhs0 = e.rhs0;
      }
      parts.push_back(separable_product(eff));
      first = 1;
    } else if (!eff.empty()) {
      parts.push_back(separable_product(eff));
    }
    for (std::size_t s = first; s < steps.size(); ++s) {
      parts.push_back(
          steps[s].cross.empty()
              ? separable_product(steps[s].sep)
              : cases_->make_event(top_, lia::btrue, steps[s].cross));
    }

    code ev = core::op_table::id;
    for (const code p : parts) ev = mgr_.operations().compose(p, ev);
    define_event(form, name, ev);
    if (ev == core::op_table::id) return;  // a no-op: skip in a seq,
                                           // nothing for the default ALT
    events_.push_back(ev);
  }

  // --- the event algebra: named terms, alt = sum, seq = compose ------------

  void define_event(const datum& at, const std::string& name, code term) {
    if (!named_events_.emplace(name, term).second) {
      fail(at, "event term '" + name + "' redefined");
    }
  }

  /// An event term: a declared name, or an inline (alt …) / (seq …).
  code read_evterm(const datum& d) {
    if (d.is_atom()) {
      const auto it = named_events_.find(d.text());
      if (it == named_events_.end()) {
        fail(d, "no event term named '" + d.text() + "'");
      }
      return it->second;
    }
    if (!d.is_list() || d.items().empty()) fail(d, "not an event term");
    const std::string& kw = d.head();
    if (d.items().size() < 2) fail(d, "'" + kw + "' needs event terms");
    if (kw == "alt") {
      std::vector<code> ops;
      for (std::size_t i = 1; i < d.items().size(); ++i) {
        ops.push_back(read_evterm(d.items()[i]));
      }
      return mgr_.operations().sum(ops);
    }
    if (kw == "seq") {  // reading order: the first term applies first
      code ev = core::op_table::id;
      for (std::size_t i = 1; i < d.items().size(); ++i) {
        ev = mgr_.operations().compose(read_evterm(d.items()[i]), ev);
      }
      return ev;
    }
    fail(d, "an event term is a name, (alt …) or (seq …)");
  }

  void do_combinator(const datum& form, bool is_alt) {
    const std::string& name = sym(arg(form, 1, "term name"));
    std::vector<datum> body(form.items().begin() + 2, form.items().end());
    if (body.empty()) fail(form, "a combinator needs event terms");
    datum inline_form = datum::list(
        [&] {
          std::vector<datum> xs{atom_datum(is_alt ? "alt" : "seq", form)};
          xs.insert(xs.end(), body.begin(), body.end());
          return xs;
        }(),
        form.line());
    define_event(form, name, read_evterm(inline_form));
  }

  static datum atom_datum(const char* text, const datum& at) {
    return datum::atom(text, at.line());
  }

  /// The summands the saturation rewrite classifies: an `alt` is seen
  /// through (its operands, recursively), anything else is one summand.
  void collect_summands(code term, std::vector<code>& out) {
    if (term == core::op_table::id) return;
    const core::op_term& t = mgr_.operations()[term];
    if (t.kind == core::op_kind::sum) {
      for (const code op : t.operands()) collect_summands(op, out);
      return;
    }
    out.push_back(term);
  }

  // --- commands ------------------------------------------------------------

  code named(const datum& d) {
    const std::string& name = sym(d);
    auto it = results_.find(name);
    if (it == results_.end()) fail(d, "no result named '" + name + "'");
    return it->second;
  }

  /// The least fixpoint of \p system (default: the ALT of every declared
  /// event) from the initial word. `naive` iterates, `saturate` applies the
  /// static closure over the flattened summands; both denote the same
  /// diagram. Propagates `hsc::overflow_error` if a leaf value leaves its
  /// representable range.
  code run_reach(bool naive, std::optional<code> system = std::nullopt) {
    std::vector<code> summands;
    if (system) {
      collect_summands(*system, summands);
    } else {
      summands = events_;
    }
    core::diagram_engine& diagrams = mgr_.diagrams();
    code reachable = initial();
    util::stopwatch sw;
    if (naive) {
      const code all = mgr_.operations().sum(summands);
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

  void do_reach(const datum& form) {
    if (top_ == core::none) fail(form, "reach before shape");
    const std::string& name = sym(arg(form, 1, "result name"));
    bool naive = false;
    std::optional<code> system;
    for (std::size_t i = 2; i < form.items().size(); ++i) {
      const datum& d = form.items()[i];
      if (d.is_atom() && d.text() == "naive") naive = true;
      else if (d.is_atom() && d.text() == "saturate") continue;
      else if (!system) system = read_evterm(d);
      else fail(d, "reach takes one event term at most");
    }
    results_[name] = run_reach(naive, system);
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

  /// `(states [RESULT])`: the cardinal, MCC-format. With a bound result
  /// (`(reach x SYSTEM)` then `(states x)`) it reads that; without one it
  /// runs the default system's reach — the MCC StateSpace examination.
  void do_states(const datum& form) {
    if (top_ == core::none) fail(form, "states before shape");
    try {
      const code r = form.items().size() > 1 ? named(form.items()[1])
                                             : run_reach(false);
      out_ << "STATE_SPACE STATES " << std::fixed << std::setprecision(0)
           << mgr_.diagrams().cardinal(r) << TECHNIQUES;
    } catch (const hsc::overflow_error& e) {
      out_ << "STATE_SPACE STATES CANNOT_COMPUTE\n";
      std::cerr << "overflow: " << e.what() << '\n';
      ++failures_;
    }
  }

  /// `(max-value NAME)`: the largest value any leaf holds in the bound
  /// result — a general statistic (MAX_TOKEN_IN_PLACE; 1-safety is
  /// `max-value <= 1`, judged by whoever asked).
  void do_max_value(const datum& form) {
    const code c = named(arg(form, 1, "result name"));
    out_ << sym(form.items()[1]) << " max-value " << max_leaf_value(c)
         << '\n';
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

  /// Arrays resolve lazily (after the shape) and cache their placement.
  mutable std::unordered_map<std::string, array_decl> arrays_;
  std::unique_ptr<case_engine> cases_;
  std::unique_ptr<expr_reader> reader_;

  std::vector<code> events_;  ///< the default system: every (event …)
  /// Every named term: events, alts, seqs — one namespace.
  std::unordered_map<std::string, code> named_events_;
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
