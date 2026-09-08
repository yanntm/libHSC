/// \file surface_events.cc
/// \brief The event compiler — the separable Presburger fragment split
/// along the separable/crossing seam — and the event algebra (named
/// terms, alt = sum, seq = compose).

#include <cstdint>

#include "surface_translator.hh"

namespace hsc::surface {

/// The predicate a single atom `(cmp leaf K)` or `(in leaf K…)` puts on
/// the leaf's coordinate (`lia` position 0) — symbolic, no domain
/// materialized. Refuses anything relating a second leaf.
lia::bexpr translator::atom_guard(const datum& atom) {
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

bool translator::is_node_kind(lia::iexpr e, lia::ikind k) const {
  return (e & 1u) == 0 && e != 0 && theory_->exprs().kind(e) == k;
}

/// Parse one action `(op LHS EXPR)`; `+=`/`-=` desugar to `:=` with the
/// target read on the right; `(havoc LHS LO HI)` is any value of the
/// range.
translator::parsed_assign translator::parse_action(const datum& act) {
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
code translator::separable_product_at(core::shape_code sort, std::size_t base,
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

code translator::separable_product(const std::map<std::size_t, leaf_effect>& eff) {
  return separable_product_at(top_, 0, eff);
}

/// A dead event still names a term — the zero term, which never fires:
/// absent from an alt, fatal to a seq.
void translator::dead_event(const datum& form, const std::string& name) {
  define_event(form, name, zero_term());
}

code translator::zero_term_at(core::shape_code sort) {
  return cases_->make_event(sort, lia::bfalse, {});
}

translator::compiled_clause translator::compile_clause(const datum& at,
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
code translator::clause_term(const compiled_clause& cc) {
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
bool translator::read_when(const datum& clause, std::map<std::size_t, leaf_effect>& eff,
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
std::vector<case_engine::assign> translator::shift_assigns(
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
code translator::compile_event_at(const datum& at, std::span<const datum> body,
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
      if (e.has_havoc) {  // fuse the whole action, not only an rhs
        g.has_havoc = true;
        g.lo = e.lo;
        g.hi = e.hi;
      } else {
        g.has_rhs = true;
        g.rhs0 = e.rhs0;
      }
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

void translator::do_event(const datum& form) {
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
  // The guard as written, for `(deadlock)`: every atom of every when clause.
  std::vector<datum> guard;
  for (const datum& clause : std::span(form.items()).subspan(2)) {
    if (clause.is_list() && !clause.items().empty() &&
        clause.head() == "when") {
      guard.insert(guard.end(), clause.items().begin() + 1,
                   clause.items().end());
    }
  }
  event_guards_.push_back(std::move(guard));
}

void translator::define_event(const datum& at, const std::string& name, code term) {
  if (!named_events_.emplace(name, term).second) {
    fail(at, "event term '" + name + "' redefined");
  }
}

/// An event term: a declared name, an inline (alt …) / (seq …), or an
/// anonymous atom — (when BEXP+) a pure filter, (do ACT+) one clause.
/// A guarded command is their seq.
code translator::read_evterm(const datum& d) {
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

void translator::do_combinator(const datum& form, bool is_alt) {
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

datum translator::atom_datum(const char* text, const datum& at) {
  return datum::atom(text, at.line());
}

/// The summands the saturation rewrite classifies: an `alt` is seen
/// through (its operands, recursively), anything else is one summand.
void translator::collect_summands(code term, std::vector<code>& out) {
  if (term == core::op_table::id) return;
  const core::op_term& t = mgr_.operations()[term];
  if (t.kind == core::op_kind::sum) {
    for (const code op : t.operands()) collect_summands(op, out);
    return;
  }
  out.push_back(term);
}

}  // namespace hsc::surface
