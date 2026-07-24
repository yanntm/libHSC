/// \file surface_translate.cc
/// \brief M2M: `datum` forms → operations on a `core::manager`.
///
/// The pass reads the surface's forms into engine calls: leaves become a shared
/// int leaf sort with per-name bounds, the shape is interned, each event is
/// compiled to one `core::product` term over the separable Presburger fragment
///, and commands run in order. A crossing atom or action (one that would
/// need `split_equiv`) is refused by name rather than mis-compiled.

#include "hsc/surface/translate.hh"

#include "hsc/surface/expand.hh"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
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
/// most one action — an assignment (a `lia` expression over the
/// coordinate, position 0; `apply_if` folds it to assign/shift where the
/// expression is one) or a havoc range.
struct leaf_effect {
  bool has_guard = false;
  lia::bexpr guard = lia::btrue;  ///< conjunction of the leaf's atoms
  bool has_rhs = false;
  lia::iexpr rhs0 = 0;  ///< x := rhs0(x), when present
  bool has_havoc = false;
  std::int32_t lo = 0;  ///< havoc: x := any value of [lo, hi)
  std::int32_t hi = 0;
};

/// A declared array: `(array NAME CELL…)` names its cell leaves in index
/// order. The cells may sit anywhere in the shape — placement resolves to
/// frontier positions on first use, after the shape is declared.
struct array_decl {
  std::vector<std::string> cells;
  bool resolved = false;
  std::vector<std::uint32_t> positions;
};

/// How a `family` form is built: `declared` — the head-folded chain by
/// recursion over the sort tree; `unfold` — every instance enumerated and
/// summed; `check` (default) — both, requiring the same code. Chosen once
/// per run by HSC_FAMILY.
enum class family_mode { check, declared, unfold };

family_mode family_mode_from_env() {
  const char* e = std::getenv("HSC_FAMILY");
  if (e == nullptr) return family_mode::check;
  const std::string v = e;
  if (v == "declared") return family_mode::declared;
  if (v == "unfold") return family_mode::unfold;
  return family_mode::check;
}

class translator final : public name_scope {
 public:
  explicit translator(std::ostream& out)
      : out_(out), fmode_(family_mode_from_env()) {
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

  /// True when \p d is an integer atom — a test, where `as_int` demands.
  static bool is_integer(const datum& d) {
    if (!d.is_atom() || d.text().empty()) return false;
    std::int32_t v = 0;
    const std::string& t = d.text();
    const auto* end = t.data() + t.size();
    const auto res = std::from_chars(t.data(), end, v);
    return res.ec == std::errc{} && res.ptr == end;
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
  /// needs `split_equiv`: parsed and understood, then declined.
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
                "split_equiv, not implemented");
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
    else if (kw == "family") do_family(form);
    else if (kw == "reach") do_reach(form);
    else if (kw == "apply") do_apply(form);
    else if (kw == "word") do_word(form);
    else if (kw == "get-witness") do_get_witness(form);
    else if (kw == "get-states") do_get_states(form);
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
    defaults_.assign(order_.size(), 0);
    for (std::size_t i = 0; i < order_.size(); ++i) {
      const leaf_decl& d = leaves_.at(order_[i]);
      if (d.bounded) defaults_[i] = d.lo;  // bounded default: LO; else 0
    }
    init_ = defaults_;
  }

  const leaf_decl& require_leaf(const datum& d) {
    const std::string& name = sym(d);
    auto it = leaves_.find(name);
    if (it == leaves_.end()) fail(d, "unknown leaf '" + name + "'");
    if (top_ == core::none) fail(d, "leaf used before the shape is declared");
    return it->second;
  }

  // --- the initial state ---------------------------------------------------
  //
  // The base word is every leaf at its declared LO (0 unbounded); the pair
  // form edits it. The event form seeds by applying an event term ONCE to
  // the base — not a step of the transition relation, applied before the
  // closure. With alt / havoc / guards it declares initial *regions*.

  void do_init(const datum& form) {
    if (top_ == core::none) fail(form, "init before shape");
    if (form.items().size() == 1) return;  // (init): the base word as is
    const bool pairs =
        form.items().size() > 1 && form.items()[1].is_list() &&
        form.items()[1].items().size() == 2 &&
        form.items()[1].items()[0].is_atom() &&
        leaves_.contains(form.items()[1].items()[0].text());
    if (pairs) {
      if (seed_override_) fail(form, "pair init after an init event");
      for (std::size_t i = 1; i < form.items().size(); ++i) {
        const datum& pair = form.items()[i];
        if (!pair.is_list() || pair.items().size() != 2) {
          fail(pair, "init entry must be (leaf value)");
        }
        const leaf_decl& d = require_leaf(pair.items()[0]);
        init_[d.index] = as_int(pair.items()[1]);
      }
      return;
    }
    if (form.items().size() != 2) fail(form, "init takes pairs or one event term");
    if (seed_override_) fail(form, "init event already set");
    const code seeded =
        mgr_.diagrams().apply_local(read_evterm(form.items()[1]), initial());
    if (seeded == core::none) {
      fail(form, "init event produced no state: the initial set is empty, "
                 "the model is malformed");
    }
    seed_override_ = seeded;
  }

  code build_point(core::shape_code sort, std::size_t& next,
                   const std::vector<std::int32_t>& values) {
    core::diagram_engine& diagrams = mgr_.diagrams();
    switch (mgr_.shapes().kind(sort)) {
      case core::shape_kind::unit:
        return diagrams.one();
      case core::shape_kind::leaf:
        return theory_->singleton(values[next++]);
      case core::shape_kind::pair: {
        const code head = build_point(mgr_.shapes().head(sort), next, values);
        const code tail = build_point(mgr_.shapes().tail(sort), next, values);
        return diagrams.rectangle(sort, head, tail);
      }
    }
    return core::none;
  }

  /// The base word: `init_` (declared LO defaults, edited by pair inits).
  code initial() {
    std::size_t next = 0;
    return build_point(top_, next, init_);
  }

  /// What reach and the queries start from: the init event's image when
  /// one was declared, the base word otherwise.
  code seed() { return seed_override_ ? *seed_override_ : initial(); }

  // --- events: the separable Presburger fragment ---------------------------

  /// The predicate a single atom `(cmp leaf K)` or `(in leaf K…)` puts on
  /// the leaf's coordinate (`lia` position 0) — symbolic, no domain
  /// materialized. Refuses anything relating a second leaf.
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

  /// One parsed action: an assignment (target and value as expressions
  /// over frontier positions; the target a variable or an array access) or
  /// a havoc range on a target.
  struct parsed_assign {
    lia::iexpr lhs;
    lia::iexpr rhs = 0;
    bool is_havoc = false;
    std::int32_t lo = 0;
    std::int32_t hi = 0;
  };

  [[nodiscard]] bool is_node_kind(lia::iexpr e, lia::ikind k) const {
    return (e & 1u) == 0 && e != 0 && theory_->exprs().kind(e) == k;
  }

  /// Parse one action `(op LHS EXPR)`; `+=`/`-=` desugar to `:=` with the
  /// target read on the right; `(havoc LHS LO HI)` is any value of the
  /// range.
  parsed_assign parse_action(const datum& act) {
    if (!act.is_list() || act.items().size() < 3) {
      fail(act, "an action is (:= lhs expr), (+= lhs expr), (-= lhs expr) "
                "or (havoc lhs lo hi)");
    }
    const std::string& op = act.head();
    lia::expr_factory& ex = theory_->exprs();
    const lia::iexpr lhs = reader_->read_int(act.items()[1]);
    if (!is_node_kind(lhs, lia::ikind::var) &&
        !is_node_kind(lhs, lia::ikind::array) && lhs != lia::iundef) {
      fail(act.items()[1], "assignment target is not a leaf or array cell");
    }
    if (op == "havoc") {
      if (act.items().size() != 4) fail(act, "havoc takes lhs, lo and hi");
      parsed_assign a{lhs};
      a.is_havoc = true;
      a.lo = as_int(act.items()[2]);
      a.hi = as_int(act.items()[3]);
      return a;
    }
    if (act.items().size() != 3) fail(act, "an action takes lhs and expr");
    lia::iexpr rhs = reader_->read_int(act.items()[2]);
    if (op == "+=") rhs = ex.add(lhs, rhs);
    else if (op == "-=") rhs = ex.sub(lhs, rhs);
    else if (op != ":=") fail(act, "unknown action '" + op + "'");
    return {lhs, rhs};
  }

  /// The product term of per-position separable effects, at \p sort whose
  /// frontier starts at absolute position \p base. Leaf terms are
  /// position-independent codes (guards and rhs shifted to the leaf), so
  /// the product at a sub-sort is the sub-chain of the product at the top.
  code separable_product_at(core::shape_code sort, std::size_t base,
                            const std::map<std::size_t, leaf_effect>& eff) {
    std::vector<code> by_leaf(mgr_.shapes().width(sort), core::op_table::id);
    for (const auto& [pos, e] : eff) {
      const lia::bexpr g = e.has_guard ? e.guard : lia::btrue;
      by_leaf.at(pos - base) = e.has_havoc ? theory_->havoc_if(g, e.lo, e.hi)
                               : e.has_rhs ? theory_->apply_if(g, e.rhs0)
                                           : theory_->keep_if(g);
    }
    return core::product(mgr_.operations(), mgr_.shapes(), sort, by_leaf);
  }

  code separable_product(const std::map<std::size_t, leaf_effect>& eff) {
    return separable_product_at(top_, 0, eff);
  }

  /// A dead event still names a term — the zero term, which never fires:
  /// absent from an alt, fatal to a seq.
  void dead_event(const datum& form, const std::string& name) {
    define_event(form, name, zero_term());
  }

  [[nodiscard]] code zero_term_at(core::shape_code sort) {
    return cases_->make_event(sort, lia::bfalse, {});
  }

  [[nodiscard]] code zero_term() { return zero_term_at(top_); }

  /// One do clause, compiled. Separable — every action writes one position
  /// that at most itself is read — is a product of independent leaf terms;
  /// otherwise the whole clause is one case bracket, its assignments
  /// simultaneous and its reads pre-clause. `dead` on a statically ⊥
  /// access.
  struct compiled_clause {
    std::map<std::size_t, leaf_effect> sep;
    std::vector<case_engine::assign> cross;  ///< empty when separable
    bool dead = false;
  };

  compiled_clause compile_clause(const datum& at,
                                 const std::vector<parsed_assign>& acts) {
    lia::expr_factory& ex = theory_->exprs();
    compiled_clause cc;
    bool crossing = false;
    for (const parsed_assign& a : acts) {
      if (a.lhs == lia::iundef || a.rhs == lia::iundef) {
        cc.dead = true;
        return cc;
      }
      if (!is_node_kind(a.lhs, lia::ikind::var)) {
        if (a.is_havoc) fail(at, "havoc needs a resolved single-leaf target");
        crossing = true;
        continue;
      }
      if (a.is_havoc) continue;  // single leaf, no reads: always separable
      const auto p = static_cast<std::size_t>(ex.node(a.lhs).payload);
      const auto supp = ex.support(a.rhs);
      if (!ex.array_positions(a.rhs).empty() ||
          !(supp.empty() || (supp.size() == 1 && supp[0] == p))) {
        crossing = true;
      }
    }
    if (crossing) {
      for (const parsed_assign& a : acts) {
        if (a.is_havoc) {
          fail(at, "havoc cannot share a clause with a crossing action");
        }
        cc.cross.push_back({a.lhs, a.rhs});
      }
      return cc;
    }
    for (const parsed_assign& a : acts) {
      const auto p = static_cast<std::size_t>(ex.node(a.lhs).payload);
      leaf_effect& s = cc.sep[p];
      if (s.has_rhs || s.has_havoc) {
        fail(at, "a position is assigned twice in one do clause");
      }
      if (a.is_havoc) {
        s.has_havoc = true;
        s.lo = a.lo;
        s.hi = a.hi;
      } else {
        s.has_rhs = true;
        s.rhs0 = ex.shift_positions(a.rhs, -static_cast<std::int32_t>(p));
      }
    }
    return cc;
  }

  /// The term of one compiled clause alone (an anonymous `(do …)`).
  code clause_term(const compiled_clause& cc) {
    if (cc.dead) return zero_term();
    if (!cc.cross.empty()) {
      return cases_->make_event(top_, lia::btrue, cc.cross);
    }
    return separable_product(cc.sep);
  }

  /// Read when-atoms: separable single-position atoms fuse per leaf,
  /// anything else is a crossing filter (a case bracket, applied before
  /// any action so every guard reads the pre-state). False when an atom
  /// folds dead.
  bool read_when(const datum& clause, std::map<std::size_t, leaf_effect>& eff,
                 std::vector<lia::bexpr>& filters) {
    lia::expr_factory& ex = theory_->exprs();
    for (std::size_t a = 1; a < clause.items().size(); ++a) {
      const lia::bexpr b = reader_->read_bool(clause.items()[a]);
      if (b == lia::bfalse || b == lia::bundef) return false;
      if (b == lia::btrue) continue;
      const auto supp = ex.support_bool(b);
      if (ex.array_positions_bool(b).empty() && supp.size() == 1) {
        const std::size_t p = supp[0];
        const lia::bexpr b0 =
            ex.shift_positions_bool(b, -static_cast<std::int32_t>(p));
        leaf_effect& e = eff[p];
        e.guard = e.has_guard ? ex.conj(e.guard, b0) : b0;
        e.has_guard = true;
        if (e.guard == lia::bfalse) return false;  // contradictory atoms
      } else {
        filters.push_back(b);
      }
    }
    return true;
  }

  /// Shift a crossing clause's assignments to positions relative to \p base.
  std::vector<case_engine::assign> shift_assigns(
      const std::vector<case_engine::assign>& assigns, std::size_t base) {
    if (base == 0) return assigns;
    lia::expr_factory& ex = theory_->exprs();
    std::vector<case_engine::assign> out;
    out.reserve(assigns.size());
    const auto d = -static_cast<std::int32_t>(base);
    for (const case_engine::assign& a : assigns) {
      out.push_back({ex.shift_positions(a.lhs, d), ex.shift_positions(a.rhs, d)});
    }
    return out;
  }

  /// \brief The whole event-body compile — when/do \p body → one term — at
  /// \p sort, whose frontier starts at absolute position \p base.
  ///
  /// Guard atoms and do clauses are kept in order as atomic sequential
  /// steps; assembly composes the crossing filters, the product fusing
  /// separable guards with the first clause's separable assignments, then
  /// each remaining step. Every position the body touches must lie inside
  /// the sort's frontier; leaf terms are position-independent, filters and
  /// case brackets shift by −base. Sets \p dead (and returns the zero term
  /// at the sort) when a guard or action folds dead. `do_event` is the
  /// (top, 0) instance; a family straddler compiles at its block.
  code compile_event_at(const datum& at, std::span<const datum> body,
                        core::shape_code sort, std::size_t base, bool& dead) {
    dead = false;
    std::map<std::size_t, leaf_effect> eff;
    std::vector<lia::bexpr> filters;
    std::vector<std::vector<parsed_assign>> clauses;
    for (const datum& clause : body) {
      const std::string& kw = clause.head();
      if (kw == "when") {
        if (!read_when(clause, eff, filters)) {
          dead = true;
          return zero_term_at(sort);
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

    // Compile each do clause (compile_clause: separable product of leaf
    // terms, or the whole clause as one case bracket).
    std::vector<compiled_clause> steps;
    for (const auto& acts : clauses) {
      compiled_clause cc = compile_clause(at, acts);
      if (cc.dead) {
        dead = true;
        return zero_term_at(sort);
      }
      steps.push_back(std::move(cc));
    }

    lia::expr_factory& ex = theory_->exprs();
    std::vector<code> parts;
    for (const lia::bexpr b : filters) {
      const lia::bexpr sb =
          base == 0 ? b
                    : ex.shift_positions_bool(
                          b, -static_cast<std::int32_t>(base));
      parts.push_back(cases_->make_event(sort, sb, {}));
    }
    std::size_t first = 0;
    if (!steps.empty() && steps.front().cross.empty()) {
      for (auto& [p, e] : steps.front().sep) {
        leaf_effect& g = eff[p];
        g.has_rhs = true;
        g.rhs0 = e.rhs0;
      }
      parts.push_back(separable_product_at(sort, base, eff));
      first = 1;
    } else if (!eff.empty()) {
      parts.push_back(separable_product_at(sort, base, eff));
    }
    for (std::size_t s = first; s < steps.size(); ++s) {
      parts.push_back(steps[s].cross.empty()
                          ? separable_product_at(sort, base, steps[s].sep)
                          : cases_->make_event(
                                sort, lia::btrue,
                                shift_assigns(steps[s].cross, base)));
    }

    if (parts.size() > 1 &&
        mgr_.shapes().kind(sort) != core::shape_kind::pair) {
      fail(at, "a multi-step event compiled at a single leaf: composition "
               "needs a composite sort");
    }
    code ev = core::op_table::id;
    for (const code p : parts) ev = mgr_.operations().compose(p, ev);
    return ev;
  }

  void do_event(const datum& form) {
    if (top_ == core::none) fail(form, "event before shape");
    const std::string& name = sym(arg(form, 1, "event name"));
    bool dead = false;
    const code ev = compile_event_at(
        form, std::span(form.items()).subspan(2), top_, 0, dead);
    if (dead) {
      dead_event(form, name);
      return;
    }
    define_event(form, name, ev);
    if (ev == core::op_table::id) return;  // a no-op: skip in a seq,
                                           // nothing for the default ALT
    events_.push_back(ev);
  }

  // --- certified uniform families: the declared route ----------------------

  /// One `(at@ ARRAY δ)` marker of a family body: a certified access to
  /// the cell of component (i+δ) mod N.
  struct fam_access {
    std::string arr;
    long long delta = 0;
  };

  static void collect_markers(const datum& d, std::vector<fam_access>& out) {
    if (!d.is_list() || d.items().empty()) return;
    if (d.head() == "at@") {
      out.push_back({d.items()[1].text(), std::stoll(d.items()[2].text())});
      return;
    }
    for (const datum& k : d.items()) collect_markers(k, out);
  }

  /// Materialize instance \p i of a family datum: `(at@ a δ)` becomes the
  /// cell atom of component (i+δ) mod n; everything else is untouched.
  datum instantiate(const datum& d, long long i, long long n) {
    if (!d.is_list() || d.items().empty()) return d;
    if (d.head() == "at@") {
      const array_decl& a = arrays_.at(d.items()[1].text());
      const long long c = ((i + std::stoll(d.items()[2].text())) % n + n) % n;
      return datum::atom(a.cells[static_cast<std::size_t>(c)], d.line());
    }
    std::vector<datum> kids;
    kids.reserve(d.items().size());
    for (const datum& k : d.items()) kids.push_back(instantiate(k, i, n));
    return datum::list(std::move(kids), d.line());
  }

  /// What the fold recursion walks: the family body, and each instance's
  /// frontier extent. `period` is the layout's cell count per component —
  /// the memo alignment.
  struct fam_geometry {
    const datum* at = nullptr;
    std::span<const datum> body;
    long long n = 0;
    long long period = 1;
    std::vector<std::size_t> min_pos, max_pos;
  };

  /// Instance \p i compiled at \p sort (frontier from \p lo). Deadness was
  /// decided once on the representative — the family is uniform.
  code fam_instance_at(const fam_geometry& g, long long i, core::shape_code s,
                       std::size_t lo) {
    std::vector<datum> inst;
    inst.reserve(g.body.size());
    for (const datum& cl : g.body) inst.push_back(instantiate(cl, i, g.n));
    bool dead = false;
    return compile_event_at(*g.at, inst, s, lo, dead);
  }

  using fam_memo = std::map<std::pair<core::shape_code, long long>, code>;

  /// \brief The declared fold: the sum of instances
  /// \p ids — each with every cell in [lo, lo+width) — as a head-folded
  /// chain built by recursion over the sort tree, no site enumerated at
  /// shared blocks.
  ///
  /// An instance straddling this cut is compiled here, flat; the sides
  /// recurse. Memoized on (sort, lo mod period): equal sorts at equal
  /// alignment hold translation-conjugate instances, whose terms are equal
  /// codes by currification (the wrap-around instance straddles the root
  /// cut, whose sort no inner block shares). The result is code-for-code
  /// the `sum_at` of the enumerated instances — check mode asserts it.
  code fold_family(const fam_geometry& g, core::shape_code s, std::size_t lo,
                   std::vector<long long>&& ids, fam_memo& memo) {
    if (ids.empty()) return core::op_table::id;
    const auto key =
        std::make_pair(s, static_cast<long long>(lo) % g.period);
    const auto hit = memo.find(key);
    if (hit != memo.end()) return hit->second;
    code result = core::op_table::id;
    if (mgr_.shapes().kind(s) != core::shape_kind::pair) {
      // whole instances inside one leaf: theory terms, and the theory owns
      // their sum (the leaf case of sum_at)
      core::support_algebra& algebra = mgr_.algebra(s);
      for (const long long i : ids) {
        const code t = fam_instance_at(g, i, s, lo);
        result =
            result == core::op_table::id ? t : algebra.term_sum(result, t);
      }
    } else {
      const core::shape_code h = mgr_.shapes().head(s);
      const core::shape_code t = mgr_.shapes().tail(s);
      const std::size_t mid = lo + mgr_.shapes().width(h);
      std::vector<long long> hs, ts;
      std::vector<code> parts;
      for (const long long i : ids) {
        if (g.max_pos[static_cast<std::size_t>(i)] < mid) {
          hs.push_back(i);
        } else if (g.min_pos[static_cast<std::size_t>(i)] >= mid) {
          ts.push_back(i);
        } else {
          parts.push_back(fam_instance_at(g, i, s, lo));
        }
      }
      const code hc = fold_family(g, h, lo, std::move(hs), memo);
      const code tc = fold_family(g, t, mid, std::move(ts), memo);
      if (hc != core::op_table::id) {
        parts.push_back(mgr_.operations().node(hc, core::op_table::id));
      }
      if (tc != core::op_table::id) {
        parts.push_back(mgr_.operations().node(core::op_table::id, tc));
      }
      result = mgr_.operations().sum(parts);
    }
    memo.emplace(key, result);
    return result;
  }

  /// `(family NAME N CLAUSE…)`: a certified uniform family, one term for
  /// all N instances. Routes: `declared` builds the head-folded chain by
  /// recursion (O(distinct blocks) term constructions); `unfold` compiles
  /// every instance and `sum_at`s the list; `check` — the default — does
  /// both and requires the same code, which canonicity makes an exact
  /// gate. A layout that is not index-periodic falls back to unfold with
  /// a note.
  void do_family(const datum& form) {
    if (top_ == core::none) fail(form, "family before shape");
    const std::string& name = sym(arg(form, 1, "family name"));
    const long long n = as_int(arg(form, 2, "family size"));
    if (n <= 0) fail(form, "family size must be positive");
    const std::span<const datum> body = std::span(form.items()).subspan(3);

    fam_geometry g{&form, body, n, 1, {}, {}};

    // The representative decides deadness once: the family is uniform.
    bool dead = false;
    {
      std::vector<datum> inst;
      for (const datum& cl : body) inst.push_back(instantiate(cl, 0, n));
      compile_event_at(form, inst, top_, 0, dead);
    }
    if (dead) {
      dead_event(form, name);
      return;
    }

    std::vector<fam_access> acc;
    for (const datum& cl : body) collect_markers(cl, acc);

    // C4 — index-periodic layout: every family array's cells sit at
    // pos(a_i) = pos(a_0) + i·period, one period for all. The certificate
    // upstream checked the indexing; the layout is this file's shape.
    bool foldable = !acc.empty();
    long long period = 0;
    std::map<std::string, std::vector<std::uint32_t>> pos;
    for (const fam_access& a : acc) {
      if (pos.contains(a.arr)) continue;
      const auto resolved = array(a.arr);
      if (!resolved || resolved->size() != static_cast<std::size_t>(n)) {
        fail(form, "family '" + name + "': array '" + a.arr +
                       "' does not have " + std::to_string(n) + " cells");
      }
      pos.emplace(a.arr, *resolved);
    }
    for (const auto& [arr, ps] : pos) {
      if (n == 1) continue;
      const long long step = static_cast<long long>(ps[1]) - ps[0];
      if (step <= 0) {
        foldable = false;
        break;
      }
      if (period == 0) period = step;
      if (step != period) {
        foldable = false;
        break;
      }
      for (long long i = 0; i < n; ++i) {
        if (ps[static_cast<std::size_t>(i)] !=
            ps[0] + static_cast<std::uint32_t>(i * step)) {
          foldable = false;
          break;
        }
      }
      if (!foldable) break;
    }
    if (period <= 0) period = 1;
    g.period = period;

    // Per-instance frontier extents, wrap included as it falls.
    if (!acc.empty()) {
      g.min_pos.assign(static_cast<std::size_t>(n), SIZE_MAX);
      g.max_pos.assign(static_cast<std::size_t>(n), 0);
      for (long long i = 0; i < n; ++i) {
        for (const fam_access& a : acc) {
          const long long c = ((i + a.delta) % n + n) % n;
          const std::size_t p = pos.at(a.arr)[static_cast<std::size_t>(c)];
          auto& mn = g.min_pos[static_cast<std::size_t>(i)];
          auto& mx = g.max_pos[static_cast<std::size_t>(i)];
          mn = std::min(mn, p);
          mx = std::max(mx, p);
        }
      }
    }

    code term;
    if (acc.empty()) {
      // no indexed access: all instances are one code already
      term = fam_instance_at(g, 0, top_, 0);
    } else {
      const bool want_unfold = !foldable || fmode_ != family_mode::declared;
      const bool want_fold = foldable && fmode_ != family_mode::unfold;
      code unfolded = core::none;
      code folded = core::none;
      if (want_unfold) {
        std::vector<code> ts;
        ts.reserve(static_cast<std::size_t>(n));
        for (long long i = 0; i < n; ++i) {
          ts.push_back(fam_instance_at(g, i, top_, 0));
        }
        unfolded = core::sum_at(mgr_, top_, ts);
      }
      if (want_fold) {
        std::vector<long long> all(static_cast<std::size_t>(n));
        std::iota(all.begin(), all.end(), 0);
        fam_memo memo;
        folded = fold_family(g, top_, 0, std::move(all), memo);
      }
      if (want_unfold && want_fold && folded != unfolded) {
        fail(form, "family '" + name +
                       "': declared fold and enumerated sum disagree — "
                       "fold gate violated, report this");
      }
      if (!foldable) {
        std::cerr << "note: family '" << name
                  << "': layout is not index-periodic; enumerated (line "
                  << form.line() << ")\n";
      }
      term = want_fold ? folded : unfolded;
    }

    define_event(form, name, term);
    if (term == core::op_table::id) return;
    events_.push_back(term);
  }

  // --- the event algebra: named terms, alt = sum, seq = compose ------------

  void define_event(const datum& at, const std::string& name, code term) {
    if (!named_events_.emplace(name, term).second) {
      fail(at, "event term '" + name + "' redefined");
    }
  }

  /// An event term: a declared name, an inline (alt …) / (seq …), or an
  /// anonymous atom — (when BEXP+) a pure filter, (do ACT+) one clause.
  /// A guarded command is their seq.
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
    if (kw == "when") {
      std::map<std::size_t, leaf_effect> eff;
      std::vector<lia::bexpr> filters;
      if (!read_when(d, eff, filters)) return zero_term();
      code ev = core::op_table::id;
      for (const lia::bexpr b : filters) {
        ev = mgr_.operations().compose(cases_->make_event(top_, b, {}), ev);
      }
      if (!eff.empty()) {
        ev = mgr_.operations().compose(separable_product(eff), ev);
      }
      return ev;
    }
    if (kw == "do") {
      std::vector<parsed_assign> acts;
      for (std::size_t a = 1; a < d.items().size(); ++a) {
        acts.push_back(parse_action(d.items()[a]));
      }
      return clause_term(compile_clause(d, acts));
    }
    if (kw == "abort") {
      // the zero term: no successors — nothing in an alt, death in a seq
      if (d.items().size() != 1) fail(d, "(abort) takes no arguments");
      return zero_term();
    }
    if (d.items().size() < 2) fail(d, "'" + kw + "' needs event terms");
    if (kw == "alt") {
      std::vector<code> ops;
      for (std::size_t i = 1; i < d.items().size(); ++i) {
        ops.push_back(read_evterm(d.items()[i]));
      }
      // head-folded: a family of one-sided instances becomes a chain
      // mirroring the shape, one code, O(depth) summands per level
      return core::sum_at(mgr_, top_, ops);
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
  /// event) from \p from (default: the seed). `naive` iterates, `saturate`
  /// applies the static closure over the flattened summands; both denote
  /// the same diagram. Propagates `hsc::overflow_error` if a leaf value
  /// leaves its representable range.
  code run_reach(bool naive, std::optional<code> system = std::nullopt,
                 std::optional<code> from = std::nullopt) {
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

  void do_reach(const datum& form) {
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
  void do_apply(const datum& form) {
    if (top_ == core::none) fail(form, "apply before shape");
    const std::string& name = sym(arg(form, 1, "result name"));
    const code term = read_evterm(arg(form, 2, "event term"));
    const code src = named(arg(form, 3, "source result"));
    results_[name] = mgr_.diagrams().apply_local(term, src);
  }

  /// `(word NAME (LEAF VAL)*)`: bind one state as a result — unlisted
  /// leaves at their declared LO (0 unbounded). The syntax `get-witness`
  /// prints, so exhibited states round-trip.
  void do_word(const datum& form) {
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

  // --- exhibiting states ---------------------------------------------------

  /// Walk one word of \p c (first arc, first element throughout).
  bool first_word(core::shape_code sort, code c,
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
  void print_word(const std::vector<std::int32_t>& values) {
    out_ << '(';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out_ << ' ';
      out_ << '(' << order_[i] << ' ' << values[i] << ')';
    }
    out_ << ')';
  }

  /// `(get-witness NAME)`: one state of a bound result, in word syntax —
  /// the SMT get-model analogue (nonempty is sat, this is its model).
  void do_get_witness(const datum& form) {
    const code c = named(arg(form, 1, "result name"));
    out_ << sym(form.items()[1]) << " witness ";
    std::vector<std::int32_t> values;
    if (first_word(top_, c, values)) print_word(values);
    else out_ << "none";  // unsat: no state to exhibit
    out_ << '\n';
  }

  /// DFS up to \p limit complete words of \p c, emitting each.
  void enum_words(core::shape_code sort, code c,
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
  void do_get_states(const datum& form) {
    const code c = named(arg(form, 1, "result name"));
    std::size_t limit = 10;
    if (form.items().size() > 2) {
      limit = static_cast<std::size_t>(as_int(form.items()[2]));
    }
    out_ << sym(form.items()[1]) << ' ' << std::fixed << std::setprecision(0)
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

  /// One query atom applied to \p src. Two fast paths keep their dedicated
  /// resolution: a leaf against a constant (or `in`) is separable, a
  /// symbolic per-position meet (`select_where`); a leaf against a second
  /// leaf is the crossing comparison (`select_compare`). Any other BEXP —
  /// conjunction, disjunction, negation, arithmetic — compiles exactly as
  /// an event guard would, a `(when ATOM)` filter applied once: separable
  /// pieces fuse per leaf, crossing pieces become case brackets.
  code apply_atom(const datum& atom, code src) {
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
  family_mode fmode_;
  core::manager mgr_;
  core::theory_index theory_index_ = 0;
  leaves::int_set_theory* theory_ = nullptr;
  core::shape_code leaf_sort_ = core::none;

  std::unordered_map<std::string, leaf_decl> leaves_;
  std::vector<std::string> order_;  ///< leaf names in frontier order
  core::shape_code top_ = core::none;
  std::vector<std::int32_t> defaults_;  ///< every leaf at LO (0 unbounded)
  std::vector<std::int32_t> init_;      ///< the base word: defaults + pairs
  std::optional<code> seed_override_;   ///< the init event's image, once set

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

int run_file(const std::string& path, std::ostream& out, std::ostream& err,
             const std::map<std::string, long long>& params) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    err << "cannot open " << path << '\n';
    return 2;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  const std::string text = buf.str();
  try {
    const std::vector<datum> forms =
        expand(parse(text), /*families=*/true, params);
    return translate(forms, out) == 0 ? 0 : 1;
  } catch (const parse_error& e) {
    err << path << ": parse error: " << e.what() << '\n';
    return 2;
  } catch (const expand_error& e) {
    err << path << ": expand error: " << e.what() << '\n';
    return 2;
  } catch (const translate_error& e) {
    err << path << ": " << e.what() << '\n';
    return 2;
  }
}

}  // namespace hsc::surface
