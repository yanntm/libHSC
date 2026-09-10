/// \file surface_ctl.cc
/// \brief The CTL commands: `(ctl NAME FORMULA)`, `(expect-ctl NAME
/// VERDICT)`, and the deflationary closure `(gfp NAME EVTERM SOURCE)`.
///
/// The formula grammar is the atom language of `select` extended with the
/// path operators:
///
///     FORMULA ::= ATOM | true | false | (deadlock)
///               | (not F) | (and F+) | (or F+)
///               | (EX F) | (AX F) | (EF F) | (AF F) | (EG F) | (AG F)
///               | (EU F F) | (AU F F) | (EW F F) | (AW F F)
///
/// A state subformula — no path operator below it — is one selector,
/// compiled exactly as a `(when …)` filter: atoms keep their `select`
/// meaning, `and / or / not` are the surface's own. `(deadlock)` is the
/// negation of the disjunction of the declared events' guards, so it is a
/// state formula too. The checker runs the forward form (`hsc/ctl/`) over
/// the default system from the seed; the reachable set is computed once and
/// shared by every `ctl` form of the session.
#include <cstdlib>
#include <sstream>

#include "hsc/ctl/checker.hh"
#include "hsc/ctl/formula.hh"
#include "hsc/ctl/forward.hh"
#include "hsc/trace/witness.hh"
#include "surface_translator.hh"

namespace hsc::surface {

/// Per-session CTL state: the formula DAG (shared across properties, so a
/// common subformula is one node and one memo), the converter, the atoms
/// as the data they were written as, and the verdicts by name.
struct translator::ctl_state {
  ctl::formulas forms;
  ctl::forward fw{forms};
  std::vector<datum> atoms;  ///< atom index → its query atom
  std::unordered_map<std::string, std::uint32_t> atom_index;  ///< by text
  std::unordered_map<std::string, ctl::verdict> verdicts;
  std::unordered_map<std::string, ctl::forward_form> converted;  ///< by property, for `witness`
  std::optional<std::vector<code>> raw_preds;  ///< the unprotected converses, for the paths
  std::optional<code> reach;  ///< `R`, computed on first use
  /// `R` was cut by the deadline: an under-approximation. Only an
  /// existential formula answered TRUE or a universal one answered FALSE
  /// stands on it (`ctl/algorithm.md` §7); the rest is reported UNKNOWN.
  bool reach_partial = false;
  std::optional<code> dead;   ///< the reachable deadlocks, once `reach` is
  /// The inverted events against `R`, exact ones raw and the others
  /// protected by `within(R)`; empty when some event has no converse.
  /// Computed the first time a formula needs a backward operator.
  std::optional<std::vector<code>> pred;
  /// The model and the checker, built once: memos (sets, Sat, closures,
  /// the cycle test) are shared by every property of the session.
  std::unique_ptr<ctl::model> model;
  std::unique_ptr<ctl::checker> checker;
  int pred_line = 0;  ///< the line of the form that first inverted, for notes
};

translator::ctl_state& translator::ctl() {
  if (!ctl_) {
    ctl_ = std::make_shared<ctl_state>();
    const char* fast = std::getenv("HSC_CTL_SCCFAST");
    mgr_.diagrams().set_fast_cycle_witness(fast != nullptr && std::string(fast) == "1");
  }
  return *ctl_;
}

/// The written form of \p d, the key atoms are shared by.
static std::string datum_text(const datum& d) {
  std::ostringstream os;
  write(os, d);
  return os.str();
}

/// `(deadlock)`: no declared event enabled — `(not (or G_1 … G_n))` over
/// the events' guard conjunctions; an event without a guard is always
/// enabled and makes it `false`.
ctl::node_id translator::deadlock_formula(const datum& at) {
  if (!guards_complete_) {
    fail(at, "(deadlock) is not available with families: their guards are "
             "not enumerable as atoms");
  }
  ctl_state& st = ctl();
  std::vector<datum> guards;
  for (const std::vector<datum>& g : event_guards_) {
    if (g.empty()) return st.forms.constant(false);
    std::vector<datum> items{atom_datum("and", at)};
    items.insert(items.end(), g.begin(), g.end());
    guards.push_back(datum::list(std::move(items), at.line()));
  }
  if (guards.empty()) return st.forms.constant(true);  // no event: all dead
  std::vector<datum> disj{atom_datum("or", at)};
  disj.insert(disj.end(), guards.begin(), guards.end());
  const datum d = datum::list(
      {atom_datum("not", at), datum::list(std::move(disj), at.line())},
      at.line());
  return ctl_atom(d);
}

ctl::node_id translator::ctl_atom(const datum& d) {
  ctl_state& st = ctl();
  const std::string key = datum_text(d);
  auto it = st.atom_index.find(key);
  if (it == st.atom_index.end()) {
    it = st.atom_index
             .emplace(key, static_cast<std::uint32_t>(st.atoms.size()))
             .first;
    st.atoms.push_back(d);
  }
  return st.forms.atom(it->second);
}

ctl::node_id translator::read_formula(const datum& d) {
  ctl_state& st = ctl();
  if (d.is_atom()) {
    if (d.text() == "true") return st.forms.constant(true);
    if (d.text() == "false") return st.forms.constant(false);
    fail(d, "a CTL formula is a list, or true / false");
  }
  if (d.items().empty()) fail(d, "empty formula");
  const std::string& kw = d.head();
  const auto kid = [&](std::size_t i) {
    return read_formula(arg(d, i, "formula operand"));
  };
  const auto exactly = [&](std::size_t n) {
    if (d.items().size() != n + 1) {
      fail(d, kw + " takes " + std::to_string(n) + " operand(s)");
    }
  };
  if (kw == "deadlock") {
    exactly(0);
    return deadlock_formula(d);
  }
  if (kw == "not") {
    exactly(1);
    return st.forms.negation(kid(1));
  }
  if (kw == "and" || kw == "or") {
    std::vector<ctl::node_id> ks;
    for (std::size_t i = 1; i < d.items().size(); ++i) ks.push_back(kid(i));
    return kw == "and" ? st.forms.conj(ks) : st.forms.disj(ks);
  }
  static const std::pair<const char*, ctl::op> unary[] = {
      {"EX", ctl::op::ex}, {"AX", ctl::op::ax}, {"EF", ctl::op::ef},
      {"AF", ctl::op::af}, {"EG", ctl::op::eg}, {"AG", ctl::op::ag}};
  for (const auto& [name, o] : unary) {
    if (kw == name) {
      exactly(1);
      return st.forms.unary(o, kid(1));
    }
  }
  static const std::pair<const char*, ctl::op> binary[] = {
      {"EU", ctl::op::eu}, {"AU", ctl::op::au}, {"EW", ctl::op::ew},
      {"AW", ctl::op::aw}};
  for (const auto& [name, o] : binary) {
    if (kw == name) {
      exactly(2);
      return st.forms.binary(o, kid(1), kid(2));
    }
  }
  // Anything else is a query atom, in the language of `select`.
  return ctl_atom(d);
}

/// The selector term of a state formula: rendered back to the atom
/// language (atoms as written, `and / or / not` as the surface spells
/// them) and compiled as the filter `(when F)`.
code translator::state_selector(ctl::node_id f) {
  ctl_state& st = ctl();
  const std::string text = st.forms.print(
      f, [&](std::uint32_t a) { return datum_text(st.atoms[a]); });
  const std::vector<datum> parsed = parse("(when " + text + ")");
  return read_evterm(parsed.front());
}

void translator::do_ctl(const datum& form) {
  if (top_ == core::none) fail(form, "ctl before shape");
  const std::string& name = sym(arg(form, 1, "property name"));
  const ctl::node_id phi = read_formula(arg(form, 2, "formula"));
  if (form.items().size() > 3) fail(form, "ctl takes one formula");
  ctl_state& st = ctl();
  try {
  if (!st.reach) {
    st.reach = run_reach(false);
    st.reach_partial = mgr_.partial();
    if (st.reach_partial) out_ << "ctl: the reachable set is partial, verdicts restricted\n";
    st.dead = core::none;
    if (guards_complete_) {
      const ctl::node_id dl = deadlock_formula(form);
      const ctl::fnode& n = st.forms[dl];
      if (n.kind == ctl::op::atom) {
        st.dead = apply_atom(st.atoms[n.atom], *st.reach);
      } else if (n.kind == ctl::op::tru) {
        st.dead = *st.reach;
      }
    }
  }
  if (!st.model) {
    auto m = std::make_unique<ctl::model>();
    m->sort = top_;
    m->reach = *st.reach;
    m->init = seed();
    m->next_events = events_;
    if (idle_event_) m->next_events.push_back(core::op_table::id);  // the self-loop
    m->pred_events = [this, &st]() -> std::span<const code> {
      if (!st.pred) {
        st.raw_preds = std::vector<code>{};
        st.pred = invert_events(datum::list({}, st.pred_line), *st.reach, &*st.raw_preds);
        if (idle_event_ && !st.pred->empty()) {
          st.pred->push_back(core::op_table::id);  // self-converse
          st.raw_preds->push_back(core::op_table::id);
        }
      }
      return *st.pred;
    };
    m->raw_pred_events = [&st]() -> std::span<const code> {
      st.model->pred_events();
      return *st.raw_preds;
    };
    m->selector = [this](ctl::node_id f) { return state_selector(f); };
    m->dead = *st.dead;
    st.model = std::move(m);
    st.checker = std::make_unique<ctl::checker>(mgr_, *st.model, st.fw);
  }
  st.pred_line = form.line();
  const ctl::forward_form ff = st.fw.convert(phi);
  st.converted[name] = ff;
    ctl::verdict v = st.checker->check(ff);
    if (st.reach_partial && v != ctl::verdict::unknown) {
      // Every set computed within a partial `R` under-approximates the
      // states satisfying its formula when no negation stands over a path
      // operator; a witness found there is real, a refutation is not.
      const ctl::formulas::quantifiers q = st.forms.path_quantifiers(st.forms.nnf(phi));
      const bool sound = (v == ctl::verdict::yes && q == ctl::formulas::quantifiers::existential) ||
                         (v == ctl::verdict::no && q == ctl::formulas::quantifiers::universal);
      if (!sound) v = ctl::verdict::unknown;
    }
    st.verdicts[name] = v;
    out_ << name << " ctl " << ctl::name(v) << '\n';
  } catch (const interrupted&) {
    // The deadline, anywhere in the form: what was memoised stays; asked
    // again it resumes there. A model cut while being built is dropped
    // whole (its deadlocks or its seed would be missing), rebuilt next time.
    if (!st.model) {
      st.reach.reset();
      st.dead.reset();
      st.reach_partial = false;
    }
    st.verdicts[name] = ctl::verdict::unknown;
    out_ << name << " ctl TIMEOUT\n";
  }
}

void translator::do_expect_ctl(const datum& form) {
  const std::string& name = sym(arg(form, 1, "property name"));
  const std::string& want = sym(arg(form, 2, "TRUE, FALSE or UNKNOWN"));
  if (want != "TRUE" && want != "FALSE" && want != "UNKNOWN") {
    fail(form, "expect-ctl wants TRUE, FALSE or UNKNOWN");
  }
  ctl_state& st = ctl();
  const auto it = st.verdicts.find(name);
  if (it == st.verdicts.end()) fail(form, "no ctl property named '" + name + "'");
  const std::string got = ctl::name(it->second);
  if (got == want) {
    out_ << "ok " << name << " " << want << '\n';
  } else {
    out_ << "FAIL " << name << " expected " << want << " got " << got << '\n';
    ++failures_;
  }
}

/// The inverted default system against the potential \p reach
/// (`core/algorithm.md` §9): each event's converse, raw when it never leaves
/// `R` on `R`, else composed with `within(R)`. An event without a converse
/// (a case bracket that assigns) leaves the whole list empty, with a note:
/// the checker then refuses backward operators rather than guess.
std::vector<code> translator::invert_events(const datum& at, code reach,
                                            std::vector<code>* raw) {
  core::inverter inv(mgr_);
  core::diagram_engine& diagrams = mgr_.diagrams();
  std::vector<code> preds;
  std::size_t protected_count = 0;
  // HSC_CTL_PROTECT: `test` (default) protects the events whose inverse
  // leaves R on R; `never` protects none — sound for verdicts relative to R
  // (an exact converse yields true predecessors, and no reachable state has
  // an unreachable successor), at the price of spurious states carried;
  // `always` protects every event. A variation point to measure.
  static const std::string protect = [] {
    const char* e = std::getenv("HSC_CTL_PROTECT");
    return e == nullptr ? std::string("test") : std::string(e);
  }();
  for (const code ev : events_) {
    code p = core::none;
    try {
      p = inv(top_, ev, reach);
    } catch (const unsupported_error& e) {
      out_ << "ctl: no backward operators (" << e.what() << ")\n";
      if (raw != nullptr) raw->clear();
      return {};
    }
    if (raw != nullptr) raw->push_back(p);
    bool guard = protect == "always";
    if (protect == "test") {
      const code img = diagrams.apply_local(p, reach);
      guard = diagrams.minus(img, reach) != core::none;
    }
    if (guard) {
      p = mgr_.operations().compose(mgr_.operations().within(reach), p);
      ++protected_count;
    }
    preds.push_back(p);
  }
  (void)at;
  if (protected_count != 0) {
    out_ << "ctl: " << protected_count << " of " << events_.size()
         << " inverted events protected by the reachable set\n";
  }
  return preds;
}

/// `(invert NAME EVTERM POTENTIAL)`: declare NAME as the converse of an
/// event term relative to a bound result — usable wherever an event term
/// stands (`apply`, `reach … from`, `gfp`).
void translator::do_invert(const datum& form) {
  if (top_ == core::none) fail(form, "invert before shape");
  const std::string& name = sym(arg(form, 1, "term name"));
  const code ev = read_evterm(arg(form, 2, "event term"));
  const code pot = named(arg(form, 3, "potential result"));
  try {
    define_event(form, name, core::inverter(mgr_)(top_, ev, pot));
  } catch (const unsupported_error& e) {
    fail(form, std::string("cannot invert: ") + e.what());
  }
}

/// `(gfp NAME EVTERM SOURCE)`: the greatest fixpoint of `X ↦ X ∩ EV(X)`
/// below a bound result — the states of SOURCE reached from a cycle inside
/// SOURCE when EV is the system's step.
void translator::do_gfp(const datum& form) {
  const std::string& name = sym(arg(form, 1, "result name"));
  const code ev = read_evterm(arg(form, 2, "event term"));
  const code src = named(arg(form, 3, "source result"));
  results_[name] =
      mgr_.diagrams().apply_local(mgr_.operations().gfp(ev), src);
}

/// `(witness NAME)`: the witness tree of a `ctl` verdict — the forward form's
/// set expressions read back as paths (`hsc/trace/witness.hh`), printed as
/// indented word literals, event names and notes. A `TRUE` verdict shows a
/// witness, a `FALSE` one the counterexample; a property that holds by
/// exhaustion has no path and says so.
void translator::do_witness(const datum& form) {
  const std::string& name = sym(arg(form, 1, "property name"));
  ctl_state& st = ctl();
  const auto it = st.converted.find(name);
  if (it == st.converted.end() || !st.checker) fail(form, "no ctl property named '" + name + "'");
  trace::graph g;
  g.sort = top_;
  g.events = st.model->next_events;
  g.preds = st.model->raw_pred_events();  // empty when there is no converse
  g.within = *st.reach;
  g.one_state = [this](code set) -> code {
    std::vector<std::int32_t> values;
    if (!first_word(top_, set, values)) return core::none;
    std::size_t next = 0;
    return build_point(top_, next, values);
  };
  std::vector<trace::witness_line> lines;
  try {
    lines = trace::witness(mgr_, *st.checker, g, it->second,
                           [&st](std::uint32_t a) { return datum_text(st.atoms[a]); });
  } catch (const interrupted&) {
    out_ << name << " witness TIMEOUT\n";
    return;
  }
  std::size_t steps = 0;
  out_ << name << " witness\n";
  for (const trace::witness_line& l : lines) {
    out_ << std::string(2 * (l.depth + 1), ' ');
    switch (l.what) {
      case trace::witness_line::kind::state: {
        std::vector<std::int32_t> values;
        first_word(top_, l.state, values);
        print_word(values);
        break;
      }
      case trace::witness_line::kind::event:
        out_ << (l.event < event_names_.size() ? event_names_[l.event] : "(idle)");
        ++steps;
        break;
      case trace::witness_line::kind::note:
        out_ << "; " << l.text;
        break;
    }
    out_ << '\n';
  }
  paths_[name] = steps;
}

}  // namespace hsc::surface
