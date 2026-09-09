/// \file surface_translator.hh
/// \brief The symbolic translator (M2M): a `datum` form at a time onto a
/// `core::manager`. Internal to `src/` — the public entry is
/// `translate()`/`run_file()` in `surface_run.cc`, the runner that also
/// knows the explicit engine; this class knows only the symbolic one.
///
/// Split across TUs by concern: declarations and dispatch
/// (`surface_translate.cc`), the event compiler and the event algebra
/// (`surface_events.cc`), certified uniform families
/// (`surface_families.cc`), commands and state exhibition
/// (`surface_query.cc`).
#pragma once

#include "hsc/surface/translate.hh"
#include "hsc/surface/expand.hh"
#include <algorithm>
#include <charconv>
#include <cstdint>
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
#include <gmpxx.h>

#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"
#include "hsc/ctl/formula.hh"
#include "hsc/event.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/query.hh"
#include "hsc/surface/expr.hh"
#include "hsc/util/errors.hh"
#include "hsc/util/timing.hh"

namespace hsc::surface {


using core::code;

/// The trailing clause every MCC answer line carries (and the newline). Our
/// verdicts come from saturated decision diagrams.
inline constexpr const char* TECHNIQUES = " TECHNIQUES DECISION_DIAGRAMS SATURATION\n";

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

inline family_mode family_mode_from_env() {
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


  /// \name the narrow API the runner drives (surface_run.cc)
  ///@{
  /// One form given meaning — the runner routes here whatever is not an
  /// explicit-engine command.
  void form(const datum& f) { dispatch(f); }
  /// The interrupt hook of the calculus (`core::manager::set_interrupt`): a
  /// long computation stops with `hsc::interrupted` when it answers true.
  void set_interrupt(std::function<bool()> hook) { mgr_.set_interrupt(std::move(hook)); }
  [[nodiscard]] int failures() const { return failures_; }
  [[nodiscard]] bool has_result(const std::string& name) const {
    return results_.contains(name);
  }
  /// Cardinal of a bound result. Precondition: `has_result`.
  [[nodiscard]] double cardinal_of(const std::string& name) {
    return mgr_.diagrams().cardinal(results_.at(name));
  }
  /// Up to \p limit words of a bound result; nullopt when absent. `limit`
  /// is exceeded loudly by returning limit+1 words when more exist.
  [[nodiscard]] std::optional<std::vector<std::vector<std::int32_t>>>
  words_of(const std::string& name, std::size_t limit) {
    const auto it = results_.find(name);
    if (it == results_.end()) return std::nullopt;
    std::vector<std::vector<std::int32_t>> out;
    std::vector<std::int32_t> acc;
    std::size_t left = limit + 1;
    enum_words(top_, it->second, acc, left, [&] {
      out.push_back(acc);
      --left;
    });
    return out;
  }
  ///@}

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

  void dispatch(const datum& form);

  void do_leaf(const datum& form);

  /// `(array NAME CELL…)`: the named leaves, in index order, are
  /// addressable as NAME[i]. Cells may sit anywhere in the shape;
  /// placement resolves on first use.
  void do_array(const datum& form);

  // --- the shape, and the frontier order it fixes --------------------------

  core::shape_code build_sort(const datum& d);

  /// `(spine a b …)` → pair(a, pair(b, … unit)). Frontier is a, b, ….
  core::shape_code build_spine(const datum& d, std::size_t from);

  /// Split in half, left-biased. Frontier is the list order.
  core::shape_code build_balanced(std::span<const datum> xs);

  void do_shape(const datum& form);

  const leaf_decl& require_leaf(const datum& d);

  // --- the initial state ---------------------------------------------------
  //
  // The base word is every leaf at its declared LO (0 unbounded); the pair
  // form edits it. The event form seeds by applying an event term ONCE to
  // the base — not a step of the transition relation, applied before the
  // closure. With alt / havoc / guards it declares initial *regions*.

  void do_init(const datum& form);

  code build_point(core::shape_code sort, std::size_t& next,
                   const std::vector<std::int32_t>& values);

  /// The base word: `init_` (declared LO defaults, edited by pair inits).
  code initial();

  /// What reach and the queries start from: the init event's image when
  /// one was declared, the base word otherwise.
  code seed() { return seed_override_ ? *seed_override_ : initial(); }

  // --- events: the separable Presburger fragment ---------------------------

  /// The predicate a single atom `(cmp leaf K)` or `(in leaf K…)` puts on
  /// the leaf's coordinate (`lia` position 0) — symbolic, no domain
  /// materialized. Refuses anything relating a second leaf.
  lia::bexpr atom_guard(const datum& atom);

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

  [[nodiscard]] bool is_node_kind(lia::iexpr e, lia::ikind k) const;

  /// Parse one action `(op LHS EXPR)`; `+=`/`-=` desugar to `:=` with the
  /// target read on the right; `(havoc LHS LO HI)` is any value of the
  /// range.
  parsed_assign parse_action(const datum& act);

  /// The product term of per-position separable effects, at \p sort whose
  /// frontier starts at absolute position \p base. Leaf terms are
  /// position-independent codes (guards and rhs shifted to the leaf), so
  /// the product at a sub-sort is the sub-chain of the product at the top.
  code separable_product_at(core::shape_code sort, std::size_t base,
                            const std::map<std::size_t, leaf_effect>& eff);

  code separable_product(const std::map<std::size_t, leaf_effect>& eff);

  /// A dead event still names a term — the zero term, which never fires:
  /// absent from an alt, fatal to a seq.
  void dead_event(const datum& form, const std::string& name);

  [[nodiscard]] code zero_term_at(core::shape_code sort);

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
                                 const std::vector<parsed_assign>& acts);

  /// The term of one compiled clause alone (an anonymous `(do …)`).
  code clause_term(const compiled_clause& cc);

  /// Read when-atoms: separable single-position atoms fuse per leaf,
  /// anything else is a crossing filter (a case bracket, applied before
  /// any action so every guard reads the pre-state). False when an atom
  /// folds dead.
  bool read_when(const datum& clause, std::map<std::size_t, leaf_effect>& eff,
                 std::vector<lia::bexpr>& filters);

  /// Shift a crossing clause's assignments to positions relative to \p base.
  std::vector<case_engine::assign> shift_assigns(
      const std::vector<case_engine::assign>& assigns, std::size_t base);

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
                        core::shape_code sort, std::size_t base, bool& dead);

  void do_event(const datum& form);

  // --- certified uniform families: the declared route ----------------------

  /// One `(at@ ARRAY δ)` marker of a family body: a certified access to
  /// the cell of component (i+δ) mod N.
  struct fam_access {
    std::string arr;
    long long delta = 0;
  };

  static void collect_markers(const datum& d, std::vector<fam_access>& out);

  /// Materialize instance \p i of a family datum: `(at@ a δ)` becomes the
  /// cell atom of component (i+δ) mod n; everything else is untouched.
  datum instantiate(const datum& d, long long i, long long n);

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
                       std::size_t lo);

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
                   std::vector<long long>&& ids, fam_memo& memo);

  /// `(family NAME N CLAUSE…)`: a certified uniform family, one term for
  /// all N instances. Routes: `declared` builds the head-folded chain by
  /// recursion (O(distinct blocks) term constructions); `unfold` compiles
  /// every instance and `sum_at`s the list; `check` — the default — does
  /// both and requires the same code, which canonicity makes an exact
  /// gate. A layout that is not index-periodic falls back to unfold with
  /// a note.
  void do_family(const datum& form);

  // --- the event algebra: named terms, alt = sum, seq = compose ------------

  void define_event(const datum& at, const std::string& name, code term);

  /// An event term: a declared name, an inline (alt …) / (seq …), or an
  /// anonymous atom — (when BEXP+) a pure filter, (do ACT+) one clause.
  /// A guarded command is their seq.
  code read_evterm(const datum& d);

  void do_combinator(const datum& form, bool is_alt);

  static datum atom_datum(const char* text, const datum& at);

  /// The summands the saturation rewrite classifies: an `alt` is seen
  /// through (its operands, recursively), anything else is one summand.
  void collect_summands(code term, std::vector<code>& out);

  // --- commands ------------------------------------------------------------

  code named(const datum& d);

  /// The least fixpoint of \p system (default: the ALT of every declared
  /// event) from \p from (default: the seed). `naive` iterates, `saturate`
  /// applies the static closure over the flattened summands; both denote
  /// the same diagram. Propagates `hsc::overflow_error` if a leaf value
  /// leaves its representable range.
  code run_reach(bool naive, std::optional<code> system = std::nullopt,
                 std::optional<code> from = std::nullopt);

  void do_reach(const datum& form);

  /// `(apply NAME EVTERM SOURCE)`: the one-step image of a bound result —
  /// the event applied once, no closure.
  void do_apply(const datum& form);

  /// `(word NAME (LEAF VAL)*)`: bind one state as a result — unlisted
  /// leaves at their declared LO (0 unbounded). The syntax `get-witness`
  /// prints, so exhibited states round-trip.
  void do_word(const datum& form);

  // --- exhibiting states ---------------------------------------------------

  /// Walk one word of \p c (first arc, first element throughout).
  bool first_word(core::shape_code sort, code c,
                  std::vector<std::int32_t>& out);

  /// Print \p values as a word literal: `((x 1) (y 0) …)` — valid as the
  /// body of `(word …)` or a pair `(init …)`, so states round-trip.
  void print_word(const std::vector<std::int32_t>& values);

  /// `(get-witness NAME)`: one state of a bound result, in word syntax —
  /// the SMT get-model analogue (nonempty is sat, this is its model).
  void do_get_witness(const datum& form);

  /// DFS up to \p limit complete words of \p c, emitting each.
  void enum_words(core::shape_code sort, code c,
                  std::vector<std::int32_t>& acc, std::size_t& left,
                  const std::function<void()>& k);

  /// `(get-states NAME [K])`: up to K states (default 10), one word
  /// literal per line, after a header with the exact cardinal.
  void do_get_states(const datum& form);

  /// The largest value any leaf holds across the diagram \p c, by a
  /// sort-directed walk: a leaf's code is a theory set to read, a pair's arcs
  /// are recursed head then tail. A general statistic (also MAX_TOKEN_IN_PLACE),
  /// not a query specialised to one-safety.
  /// \name Weighted counting (`src/surface_weighted_count.cc`)
  ///
  /// `(leaf-weight NAME K)` says a leaf stands for K places of a *free*
  /// component of the original net, one over which tokens travel freely, so
  /// a marking of v there represents C(v+K-1, K-1) markings of that net.
  /// Counting is then the ordinary recursion with one substitution: a leaf
  /// arc contributes the sum of those binomials over its values instead of
  /// how many values it holds. Its own algorithm and its own caches: no
  /// declared weight, nothing runs.
  ///@{
  void do_leaf_weight(const datum& form);
  /// Are any weights declared (and not all one)?
  [[nodiscard]] bool weighted() const { return !weights_.empty(); }
  /// The exact count of \p c with the declared weights folded in.
  [[nodiscard]] mpz_class weighted_count(code c);
  ///@}

  void collect_max(code c, core::shape_code s, std::int32_t& mx,
                   std::unordered_set<code>& seen);

  [[nodiscard]] std::int32_t max_leaf_value(code c);

  /// `(states [RESULT])`: the cardinal, MCC-format. With a bound result
  /// (`(reach x SYSTEM)` then `(states x)`) it reads that; without one it
  /// runs the default system's reach — the MCC StateSpace examination.
  void do_states(const datum& form);

  /// `(max-value NAME)`: the largest value any leaf holds in the bound
  /// result — a general statistic (MAX_TOKEN_IN_PLACE; 1-safety is
  /// `max-value <= 1`, judged by whoever asked).
  void do_max_value(const datum& form);

  /// The comparator of a query atom, by its surface spelling.
  static std::optional<cmp> comparator(const std::string& op);

  /// One query atom applied to \p src. Two fast paths keep their dedicated
  /// resolution: a leaf against a constant (or `in`) is separable, a
  /// symbolic per-position meet (`select_where`); a leaf against a second
  /// leaf is the crossing comparison (`select_compare`). Any other BEXP —
  /// conjunction, disjunction, negation, arithmetic — compiles exactly as
  /// an event guard would, a `(when ATOM)` filter applied once: separable
  /// pieces fuse per leaf, crossing pieces become case brackets.
  code apply_atom(const datum& atom, code src);

  /// `(select NAME SOURCE ATOM+)`: filter a stored result by a conjunction of
  /// query atoms and store the subset under NAME.
  void do_select(const datum& form);

  void do_count(const datum& form);


  void do_nodes(const datum& form);

  void do_print(const datum& form);

  void do_expect(const datum& form);


  void do_bill(const datum&);
  /// \name CTL (`src/surface_ctl.cc`)
  ///
  /// `(ctl NAME FORMULA)` checks a CTL formula at the seed over the default
  /// system by the forward form (`hsc/ctl/`); `(expect-ctl NAME VERDICT)`
  /// asserts its verdict; `(gfp NAME EVTERM SOURCE)` is the deflationary
  /// closure. State subformulas are selectors compiled as `(when …)`
  /// filters; `(deadlock)` is built from the declared events' guards.
  ///@{
  struct ctl_state;
  ctl_state& ctl();
  ctl::node_id read_formula(const datum& d);
  ctl::node_id ctl_atom(const datum& d);
  ctl::node_id deadlock_formula(const datum& at);
  code state_selector(ctl::node_id f);
  void do_ctl(const datum& form);
  void do_expect_ctl(const datum& form);
  void do_gfp(const datum& form);
  std::vector<code> invert_events(const datum& at, code reach);
  void do_invert(const datum& form);
  ///@}
  /// \name Paths (`src/surface_trace.cc`, `hsc/trace/`)
  ///@{
  void do_path(const datum& form);
  void do_expect_path(const datum& form);
  /// `(witness NAME)`: the witness tree of a ctl verdict (in `surface_ctl.cc`,
  /// beside the checker's state); the total of its event lines is bound like
  /// a path length, so `(expect-path NAME K)` checks it.
  void do_witness(const datum& form);
  ///@}

  // --- state ---------------------------------------------------------------

  std::ostream& out_;
  family_mode fmode_;
  core::manager mgr_;
  core::theory_index theory_index_ = 0;
  leaves::int_set_theory* theory_ = nullptr;
  core::shape_code leaf_sort_ = core::none;

  /// `(leaf-weight NAME K)` by leaf name, K > 1 only: how many places of a
  /// free component that leaf stands for. Empty in every ordinary run.
  std::map<std::string, long long> weights_;
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
  std::vector<std::string> event_names_;  ///< their names, same order
  std::unordered_map<std::string, std::size_t> paths_;  ///< `(path …)` lengths by name
  /// Every named term: events, alts, seqs — one namespace.
  std::unordered_map<std::string, code> named_events_;
  std::unordered_map<std::string, code> results_;
  /// Per declared event, the atoms of its `when` clauses (the guard as
  /// written); empty for an always-enabled event. What `(deadlock)` reads.
  std::vector<std::vector<datum>> event_guards_;
  /// False once a family entered the default system: its guards are not
  /// enumerable as atoms, so `(deadlock)` is refused.
  bool guards_complete_ = true;
  /// True once a declared event compiled to `id`: an always-enabled no-op,
  /// a self-loop on every state for the temporal operators.
  bool idle_event_ = false;
  std::shared_ptr<ctl_state> ctl_;  ///< shared: deleter typed at make time
  double reach_seconds_ = 0.0;
  int failures_ = 0;
};

}  // namespace hsc::surface
