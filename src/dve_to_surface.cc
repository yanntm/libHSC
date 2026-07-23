/// \file dve_to_surface.cc
/// \brief M2M: `dve::model` → surface forms; M2T: their `.hsc` text.

#include <cctype>
#include <cstdlib>
#include <map>
#include <ostream>

#include <string>
#include <unordered_map>
#include <vector>

#include "hsc/dve/to_surface.hh"

namespace hsc::dve {

namespace {

using surface::datum;

datum atom(const std::string& s, int line) { return datum::atom(s, line); }
datum num(std::int64_t v, int line) {
  return datum::atom(std::to_string(v), line);
}

/// The transform pass: resolution context plus emission.
class transformer {
 public:
  explicit transformer(const model& m) : m_(m) {}

  surface_model run() {
    if (!m_.async) fail(0, "system sync (lockstep) is not supported");
    fold_constants();
    declare_leaves();
    emit_init();
    emit_events();
    out_.forms.push_back(datum::list(
        {atom("reach", 0), atom("R", 0), atom("saturate", 0)}, 0));
    out_.forms.push_back(datum::list({atom("count", 0), atom("R", 0)}, 0));
    return std::move(out_);
  }

 private:
  [[noreturn]] static void fail(int line, const std::string& msg) {
    throw transform_error(line, msg);
  }

  // --- constants: compile-time integers ------------------------------------

  /// Evaluate a constant expression under the constants seen so far
  /// (\p scope names the process for local constants, "" for global).
  std::int64_t const_eval(const expr& e, const std::string& scope) {
    switch (e.kind) {
      case ekind::number:
        return e.value;
      case ekind::name: {
        if (const auto* v = find_const(scope, e.name)) return *v;
        fail(e.line, "'" + e.name + "' is not a constant");
      }
      case ekind::unary: {
        const std::int64_t a = const_eval(*e.a, scope);
        return e.op == eop::neg ? -a : e.op == eop::comp ? ~a : (a == 0 ? 1 : 0);
      }
      case ekind::binary: {
        const std::int64_t a = const_eval(*e.a, scope);
        const std::int64_t b = const_eval(*e.b, scope);
        switch (e.op) {
          case eop::add: return a + b;
          case eop::sub: return a - b;
          case eop::mul: return a * b;
          case eop::div:
            if (b == 0) fail(e.line, "constant division by zero");
            return a / b;
          case eop::mod:
            if (b == 0) fail(e.line, "constant modulo by zero");
            return a % b;
          case eop::shl: return a << b;
          case eop::shr: return a >> b;
          case eop::band: return a & b;
          case eop::bor: return a | b;
          case eop::bxor: return a ^ b;
          case eop::eq: return a == b;
          case eop::ne: return a != b;
          case eop::lt: return a < b;
          case eop::le: return a <= b;
          case eop::gt: return a > b;
          case eop::ge: return a >= b;
          case eop::and_: return (a != 0 && b != 0) ? 1 : 0;
          case eop::or_: return (a != 0 || b != 0) ? 1 : 0;
          case eop::imply: return (a == 0 || b != 0) ? 1 : 0;
          default: fail(e.line, "not a constant operator");
        }
      }
      default:
        fail(e.line, "not a constant expression");
    }
  }

  const std::int64_t* find_const(const std::string& scope,
                                 const std::string& name) const {
    if (!scope.empty()) {
      const auto it = consts_.find(scope + "\x1f" + name);
      if (it != consts_.end()) return &it->second;
    }
    const auto it = consts_.find(name);
    return it == consts_.end() ? nullptr : &it->second;
  }

  void fold_constants() {
    for (const const_decl& c : m_.constants) {
      consts_[c.name] = const_eval(*c.value, "");
    }
    for (const process& p : m_.processes) {
      for (const const_decl& c : p.constants) {
        consts_[p.name + "\x1f" + c.name] = const_eval(*c.value, p.name);
      }
    }
  }

  // --- declarations: leaves, in declaration order --------------------------

  struct leaf_info {
    std::string leaf;  ///< surface name; arrays: the prefix, cells leaf_i
    bool is_byte = false;
    bool is_array = false;
    std::int32_t size = 0;
  };

  void declare_var(const var_decl& v, const std::string& prefix,
                   const std::string& scope) {
    leaf_info info{prefix + v.name, v.is_byte, v.is_array, v.size};
    const std::string key = scope.empty() ? v.name : scope + "\x1f" + v.name;
    vars_[key] = info;
    const auto one = [&](const std::string& leaf, std::int64_t init) {
      declare_leaf(leaf, v.is_byte, v.line);
      if (init != 0) inits_.emplace_back(leaf, init);
    };
    if (v.is_array) {
      for (std::int32_t i = 0; i < v.size; ++i) {
        const std::int64_t init =
            static_cast<std::size_t>(i) < v.init_list.size()
                ? const_eval(*v.init_list[i], scope)
                : 0;
        one(info.leaf + "_" + std::to_string(i), init);
      }
      // the placement declaration: cells by name, in index order
      std::vector<datum> arr{atom("array", v.line), atom(info.leaf, v.line)};
      for (std::int32_t i = 0; i < v.size; ++i) {
        arr.push_back(atom(info.leaf + "_" + std::to_string(i), v.line));
      }
      out_.forms.push_back(datum::list(std::move(arr), v.line));
    } else {
      one(info.leaf, v.init ? const_eval(*v.init, scope) : 0);
    }
  }

  void declare_leaf(const std::string& name, bool is_byte, int line) {
    std::vector<datum> d{atom("leaf", line), atom(name, line)};
    if (is_byte) {  // the opt-in [0,256) bound: model information
      d.push_back(num(0, line));
      d.push_back(num(256, line));
    }
    out_.forms.push_back(datum::list(std::move(d), line));
    order_.push_back(name);
  }

  void declare_leaves() {
    for (const var_decl& v : m_.globals) declare_var(v, "glob_", "");
    for (const process& p : m_.processes) {
      if (p.states.empty()) fail(p.line, "process without states");
      declare_leaf(p.name + "_state", false, p.line);
      const std::int32_t init = state_index(p, p.init_state, p.line);
      if (init != 0) inits_.emplace_back(p.name + "_state", init);
      for (const var_decl& v : p.locals) {
        declare_var(v, p.name + "_", p.name);
      }
      for (const std::string& s : p.states) {
        if (s.rfind("trans_", 0) == 0) ++transients_;
      }
    }
    std::vector<datum> spine{atom("spine", 0)};
    for (const std::string& l : order_) spine.push_back(atom(l, 0));
    out_.forms.push_back(datum::list(
        {atom("shape", 0), datum::list(std::move(spine), 0)}, 0));
    if (transients_ != 0) {
      out_.notes.push_back(std::to_string(transients_) +
                           " trans_* states: DiVinE transient semantics not "
                           "reproduced");
    }
  }

  static std::int32_t state_index(const process& p, const std::string& s,
                                  int line) {
    for (std::size_t i = 0; i < p.states.size(); ++i) {
      if (p.states[i] == s) return static_cast<std::int32_t>(i);
    }
    fail(line, "unknown state '" + s + "' in process " + p.name);
  }

  void emit_init() {
    std::vector<datum> init{atom("init", 0)};
    for (const auto& [leaf, v] : inits_) {
      init.push_back(datum::list({atom(leaf, 0), num(v, 0)}, 0));
    }
    out_.forms.push_back(datum::list(std::move(init), 0));
  }

  // --- expression translation ----------------------------------------------

  const leaf_info& resolve_var(const std::string& scope,
                               const std::string& name, int line) {
    if (!scope.empty()) {
      const auto it = vars_.find(scope + "\x1f" + name);
      if (it != vars_.end()) return it->second;
    }
    const auto it = vars_.find(name);
    if (it == vars_.end()) fail(line, "unknown variable '" + name + "'");
    return it->second;
  }

  const process& resolve_process(const std::string& name, int line) const {
    for (const process& p : m_.processes) {
      if (p.name == name) return p;
    }
    fail(line, "unknown process '" + name + "'");
  }

  /// An integer-valued datum for \p e; names resolve through \p scope.
  datum to_int(const expr& e, const std::string& scope) {
    switch (e.kind) {
      case ekind::number:
        return num(e.value, e.line);
      case ekind::name: {
        if (const auto* c = find_const(scope, e.name)) return num(*c, e.line);
        const leaf_info& v = resolve_var(scope, e.name, e.line);
        if (v.is_array) fail(e.line, "array '" + e.name + "' used bare");
        return atom(v.leaf, e.line);
      }
      case ekind::array_ref: {
        const leaf_info& v = resolve_var(scope, e.name, e.line);
        if (!v.is_array) fail(e.line, "'" + e.name + "' is not an array");
        const datum idx = to_int(*e.a, scope);
        if (idx.is_atom()) {  // constant index resolves to the cell leaf
          char* end = nullptr;
          const long i = std::strtol(idx.text().c_str(), &end, 10);
          if (end != idx.text().c_str() && *end == '\0') {
            if (i < 0 || i >= v.size) {
              fail(e.line, "index " + std::to_string(i) + " out of [0," +
                               std::to_string(v.size) + ") for '" + e.name +
                               "'");
            }
            return atom(v.leaf + "_" + std::to_string(i), e.line);
          }
        }
        return datum::list({atom("at", e.line), atom(v.leaf, e.line), idx},
                           e.line);
      }
      case ekind::proc_state:  // a boolean read in integer position
        return to_bool(e, scope);
      case ekind::unary: {
        const datum a = to_int(*e.a, scope);
        if (e.op == eop::neg) {
          return datum::list({atom("-", e.line), num(0, e.line), a}, e.line);
        }
        if (e.op == eop::comp) {
          return datum::list({atom("~", e.line), a}, e.line);
        }
        return datum::list({atom("not", e.line), a}, e.line);
      }
      case ekind::binary: {
        const char* op = op_text(e.op);
        return datum::list(
            {atom(op, e.line), to_int(*e.a, scope), to_int(*e.b, scope)},
            e.line);
      }
    }
    fail(e.line, "unmapped expression");
  }

  static const char* op_text(eop op) {
    switch (op) {
      case eop::imply: return "imply";
      case eop::or_: return "or";
      case eop::and_: return "and";
      case eop::bor: return "|";
      case eop::band: return "&";
      case eop::bxor: return "^";
      case eop::eq: return "==";
      case eop::ne: return "!=";
      case eop::lt: return "<";
      case eop::le: return "<=";
      case eop::gt: return ">";
      case eop::ge: return ">=";
      case eop::shl: return "<<";
      case eop::shr: return ">>";
      case eop::add: return "+";
      case eop::sub: return "-";
      case eop::mul: return "*";
      case eop::div: return "/";
      case eop::mod: return "%";
      default: return "?";
    }
  }

  /// A boolean-valued datum for \p e: comparisons and connectives stay,
  /// `Proc.state` becomes its `==` atom, an arithmetic term becomes `!= 0`.
  datum to_bool(const expr& e, const std::string& scope) {
    switch (e.kind) {
      case ekind::proc_state: {
        const process& p = resolve_process(e.name, e.line);
        const std::int32_t k = state_index(p, e.field, e.line);
        return datum::list({atom("==", e.line),
                            atom(p.name + "_state", e.line), num(k, e.line)},
                           e.line);
      }
      case ekind::unary:
        if (e.op == eop::not_) {
          return datum::list({atom("not", e.line), to_bool(*e.a, scope)},
                             e.line);
        }
        break;
      case ekind::binary:
        switch (e.op) {
          case eop::and_:
          case eop::or_:
            return datum::list({atom(e.op == eop::and_ ? "and" : "or", e.line),
                                to_bool(*e.a, scope),
                                to_bool(*e.b, scope)},
                               e.line);
          case eop::imply:
            return datum::list(
                {atom("or", e.line),
                 datum::list({atom("not", e.line), to_bool(*e.a, scope)},
                             e.line),
                 to_bool(*e.b, scope)},
                e.line);
          case eop::eq: case eop::ne: case eop::lt: case eop::le:
          case eop::gt: case eop::ge:
            return datum::list({atom(op_text(e.op), e.line),
                                to_int(*e.a, scope),
                                to_int(*e.b, scope)},
                               e.line);
          default:
            break;
        }
        break;
      default:
        break;
    }
    // arithmetic (or a literal) in boolean position: e != 0
    return datum::list(
        {atom("!=", e.line), to_int(e, scope), num(0, e.line)}, e.line);
  }

  /// Append \p g to \p atoms, flattening top-level conjunctions. Returns
  /// false if the guard is literally false (the event is dead).
  bool flatten_guard(const datum& g, std::vector<datum>& atoms) {
    if (g.is_list() && g.head() == "and") {
      for (std::size_t i = 1; i < g.items().size(); ++i) {
        if (!flatten_guard(g.items()[i], atoms)) return false;
      }
      return true;
    }
    // (!= K 0) on literals folds here; anything else passes through
    if (g.is_list() && g.items().size() == 3 && g.items()[1].is_atom() &&
        g.items()[2].is_atom() && g.items()[2].text() == "0" &&
        g.head() == "!=") {
      const std::string& t = g.items()[1].text();
      if (t == "0") return false;
      if (!t.empty() && (std::isdigit(static_cast<unsigned char>(t[0])) ||
                         t[0] == '-')) {
        return true;  // a nonzero literal: trivially true, no atom
      }
    }
    atoms.push_back(g);
    return true;
  }

  // --- effects: sequential steps, one do-clause each -----------------------
  //
  // DVE executes an effect list left to right; the surface expresses that as
  // repeated (do …) clauses, compiled to op_table::compose. No substitution
  // into a simultaneous assign: a sync assign entangles supports and hurts
  // saturation, so it is reserved for semantics that truly need it.

  /// DVE bytes are unsigned 8-bit: an assigned value truncates mod 256.
  /// `((rhs % 256) + 256) % 256` normalizes the sign too; the surface's
  /// expression layer folds it away on constants. A literal already in
  /// range skips the wrap.
  datum wrap_byte(datum rhs, int line) {
    if (rhs.is_atom()) {
      char* end = nullptr;
      const long k = std::strtol(rhs.text().c_str(), &end, 10);
      if (end != rhs.text().c_str() && *end == '\0' && k >= 0 && k < 256) {
        return rhs;
      }
    }
    datum m = datum::list(
        {atom("%", line), std::move(rhs), num(256, line)}, line);
    datum p = datum::list(
        {atom("+", line), std::move(m), num(256, line)}, line);
    return datum::list({atom("%", line), std::move(p), num(256, line)}, line);
  }

  /// One `(:= target rhs)` form. The target resolves in \p tscope; the rhs
  /// is already translated (its scope differs at a rendezvous, where the
  /// sent value reads the sender and the variable belongs to the receiver).
  datum assign_form(const std::string& var, const expr* index,
                    const std::string& tscope, datum rhs, int line) {
    const leaf_info& v = resolve_var(tscope, var, line);
    if (v.is_byte) rhs = wrap_byte(std::move(rhs), line);
    if (index == nullptr) {
      if (v.is_array) fail(line, "array '" + var + "' assigned bare");
      return datum::list(
          {atom(":=", line), atom(v.leaf, line), std::move(rhs)}, line);
    }
    if (!v.is_array) fail(line, "'" + var + "' is not an array");
    const datum idx = to_int(*index, tscope);
    if (idx.is_atom()) {
      char* end = nullptr;
      const long i = std::strtol(idx.text().c_str(), &end, 10);
      if (end != idx.text().c_str() && *end == '\0') {
        if (i < 0 || i >= v.size) {
          fail(line, "index out of bounds writing '" + var + "'");
        }
        return datum::list({atom(":=", line),
                            atom(v.leaf + "_" + std::to_string(i), line),
                            std::move(rhs)},
                           line);
      }
    }
    return datum::list(
        {atom(":=", line),
         datum::list({atom("at", line), atom(v.leaf, line), idx}, line),
         std::move(rhs)},
        line);
  }

  static datum do_clause(datum one, int line) {
    return datum::list({atom("do", line), std::move(one)}, line);
  }

  datum move_clause(const process& p, const std::string& dest, int line) {
    return do_clause(
        datum::list({atom(":=", line), atom(p.name + "_state", line),
                     num(state_index(p, dest, line), line)},
                    line),
        line);
  }

  /// One surface event: the when-atoms, then the ordered do-clauses.
  void emit_event(const std::string& name, std::vector<datum> atoms,
                  std::vector<datum> clauses, int line) {
    std::vector<datum> when{atom("when", line)};
    for (datum& a : atoms) when.push_back(std::move(a));
    std::vector<datum> ev{atom("event", line), atom(name, line),
                          datum::list(std::move(when), line)};
    for (datum& c : clauses) ev.push_back(std::move(c));
    out_.forms.push_back(datum::list(std::move(ev), line));
  }

  /// The `when` atoms of a transition: its source-state atom plus its guard.
  /// Returns false when the guard is literally false.
  bool transition_atoms(const process& p, const transition& t,
                        std::vector<datum>& atoms) {
    atoms.push_back(datum::list(
        {atom("==", t.line), atom(p.name + "_state", t.line),
         num(state_index(p, t.src, t.line), t.line)},
        t.line));
    if (t.guard) {
      return flatten_guard(to_bool(*t.guard, p.name), atoms);
    }
    return true;
  }

  void emit_events() {
    // channel → its send and recv transitions (with their processes)
    using tref = std::pair<const process*, const transition*>;
    std::map<std::string, std::vector<tref>> sends, recvs;
    for (const process& p : m_.processes) {
      for (const transition& t : p.transitions) {
        if (t.sync == transition::sync_kind::send) {
          sends[t.channel].emplace_back(&p, &t);
        } else if (t.sync == transition::sync_kind::recv) {
          recvs[t.channel].emplace_back(&p, &t);
        }
      }
    }

    // fused rendezvous events, one per (send, recv) pair
    std::size_t dropped = 0;
    for (const auto& [chan, ss] : sends) {
      const auto rit = recvs.find(chan);
      if (rit == recvs.end()) { dropped += ss.size(); continue; }
      for (const auto& [sp, st] : ss) {
        for (const auto& [rp, rt] : rit->second) {
          if (sp == rp) continue;  // a process cannot rendezvous with itself
          std::vector<datum> atoms;
          if (!transition_atoms(*sp, *st, atoms)) continue;
          if (!transition_atoms(*rp, *rt, atoms)) continue;
          if (rt->sync_var.empty() != (st->sync_value == nullptr)) {
            fail(rt->line, "channel " + chan +
                               ": value passed on one side only");
          }
          // reference order: sender effects, sender move, the carried value,
          // receiver effects, receiver move — each an atomic step.
          std::vector<datum> clauses;
          for (const assign& a : st->effects) {
            clauses.push_back(do_clause(
                assign_form(a.var, a.index.get(), sp->name,
                            to_int(*a.rhs, sp->name), a.line),
                a.line));
          }
          clauses.push_back(move_clause(*sp, st->dest, st->line));
          if (!rt->sync_var.empty()) {
            clauses.push_back(do_clause(
                assign_form(rt->sync_var, rt->sync_index.get(), rp->name,
                            to_int(*st->sync_value, sp->name), rt->line),
                rt->line));
          }
          for (const assign& a : rt->effects) {
            clauses.push_back(do_clause(
                assign_form(a.var, a.index.get(), rp->name,
                            to_int(*a.rhs, rp->name), a.line),
                a.line));
          }
          clauses.push_back(move_clause(*rp, rt->dest, rt->line));
          emit_event(chan + "_" + sp->name + "_" + rp->name + "_t" +
                         std::to_string(ti(*sp, *st)) + "_t" +
                         std::to_string(ti(*rp, *rt)),
                     std::move(atoms), std::move(clauses), st->line);
        }
      }
    }
    for (const auto& [chan, rs] : recvs) {
      if (!sends.contains(chan)) dropped += rs.size();
    }
    if (dropped != 0) {
      out_.notes.push_back(std::to_string(dropped) +
                           " sync transitions on unmatched channels dropped");
    }

    // plain local transitions
    for (const process& p : m_.processes) {
      for (const transition& t : p.transitions) {
        if (t.sync != transition::sync_kind::none) continue;
        std::vector<datum> atoms;
        if (!transition_atoms(p, t, atoms)) continue;
        // reference order: the control move first, then the effects
        std::vector<datum> clauses{move_clause(p, t.dest, t.line)};
        for (const assign& a : t.effects) {
          clauses.push_back(do_clause(
              assign_form(a.var, a.index.get(), p.name,
                          to_int(*a.rhs, p.name), a.line),
              a.line));
        }
        emit_event(p.name + "_t" + std::to_string(ti(p, t)) + "_" + t.src +
                       "_" + t.dest,
                   std::move(atoms), std::move(clauses), t.line);
      }
    }
  }

  static std::size_t ti(const process& p, const transition& t) {
    return static_cast<std::size_t>(&t - p.transitions.data()) + 1;
  }

  // --- state ---------------------------------------------------------------

  const model& m_;
  surface_model out_;
  std::unordered_map<std::string, std::int64_t> consts_;
  std::unordered_map<std::string, leaf_info> vars_;
  std::vector<std::string> order_;
  std::vector<std::pair<std::string, std::int64_t>> inits_;
  std::size_t transients_ = 0;
};

void print_datum(std::ostream& os, const datum& d) {
  if (d.is_atom()) {
    os << d.text();
    return;
  }
  os << '(';
  bool first = true;
  for (const datum& x : d.items()) {
    if (!first) os << ' ';
    first = false;
    print_datum(os, x);
  }
  os << ')';
}

}  // namespace

surface_model to_surface(const model& m) { return transformer(m).run(); }

void print_hsc(std::ostream& os, const surface_model& sm) {
  for (const std::string& n : sm.notes) os << "; " << n << '\n';
  for (const datum& f : sm.forms) {
    print_datum(os, f);
    os << '\n';
  }
}

}  // namespace hsc::dve
