/// \file witness.cc
/// \brief The witness tree of a CTL verdict (`hsc/trace/witness.hh`,
/// `algorithm.md` §2–§3).
#include "hsc/trace/witness.hh"

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"

namespace hsc::trace {

namespace {

using core::code;
using ctl::node_id;
using ctl::set_id;

struct builder {
  core::manager& mgr;
  ctl::checker& chk;
  const graph& g;
  core::diagram_engine& diagrams;
  const std::function<std::string(std::uint32_t)>& atom_name;
  std::vector<witness_line> lines;

  void note(std::size_t depth, std::string text) {
    lines.push_back({witness_line::kind::note, depth, core::none, 0, std::move(text)});
  }
  void state(std::size_t depth, code s) {
    lines.push_back({witness_line::kind::state, depth, s, 0, {}});
  }
  void event(std::size_t depth, std::size_t j) {
    lines.push_back({witness_line::kind::event, depth, core::none, j, {}});
  }
  std::string show(node_id f) { return chk.forms().print(f, atom_name); }

  /// The lines of a run, its first state omitted (already placed).
  void run_after_first(std::size_t depth, const path_result& p) {
    for (std::size_t i = 0; i < p.events.size(); ++i) {
      event(depth, p.events[i]);
      state(depth, p.states[i + 1]);
    }
  }

  /// One step from \p t into \p target through some event: the event and the
  /// state reached, or nullopt.
  std::optional<std::pair<std::size_t, code>> step_into(code t, code target) {
    for (std::size_t j = 0; j < g.events.size(); ++j) {
      const code img = diagrams.meet(diagrams.apply_local(g.events[j], t), target);
      if (img != core::none) return std::make_pair(j, g.one_state(img));
    }
    return std::nullopt;
  }

  /// A lasso from \p t inside the set \p hull, whose every state has a
  /// successor in it: a path to a state on a cycle, then the cycle.
  void lasso(std::size_t depth, code t, code hull) {
    // Advance until a state repeats: follow successors inside the hull,
    // remembering the states seen; symbolic sets are tiny here (one state).
    std::vector<code> seen{t};
    code cur = t;
    for (std::size_t guard = 0; guard < 100000; ++guard) {
      const std::optional<std::pair<std::size_t, code>> nx = step_into(cur, hull);
      if (!nx) {
        note(depth, "no successor inside the hull (unexpected)");
        return;
      }
      event(depth, nx->first);
      state(depth, nx->second);
      cur = nx->second;
      for (const code s : seen) {
        if (s == cur) {
          note(depth, "cycle: the state above was seen before");
          return;
        }
      }
      seen.push_back(cur);
    }
    note(depth, "lasso search gave up after 100000 steps");
  }

  /// Explain a state formula or a temporal one at the state \p t (backward
  /// reading, `ctl/algorithm.md` §3).
  void formula(std::size_t depth, node_id f, code t) {
    ctl::formulas& F = chk.forms();
    const ctl::fnode n = F[f];
    if (F.is_state(f)) {
      note(depth, "holds: " + show(f));
      return;
    }
    switch (n.kind) {
      case ctl::op::and_:
        for (const node_id k : n.kids) formula(depth, k, t);
        return;
      case ctl::op::or_:
        for (const node_id k : n.kids) {
          const std::optional<code> s = chk.sat(k);
          if (s && diagrams.meet(*s, t) != core::none) {
            formula(depth, k, t);
            return;
          }
        }
        note(depth, "no disjunct holds (unexpected)");
        return;
      case ctl::op::ex: {
        const std::optional<code> s = chk.sat(n.kids[0]);
        if (!s) break;
        const auto nx = step_into(t, *s);
        if (!nx) break;
        note(depth, "EX: one step");
        event(depth, nx->first);
        state(depth, nx->second);
        formula(depth + 1, n.kids[0], nx->second);
        return;
      }
      case ctl::op::ef:
      case ctl::op::eu: {
        const node_id lf = n.kind == ctl::op::ef ? F.constant(true) : n.kids[0];
        const node_id rg = n.kind == ctl::op::ef ? n.kids[0] : n.kids[1];
        const std::optional<code> sf = chk.sat(lf);
        const std::optional<code> sg = chk.sat(rg);
        if (!sf || !sg) break;
        const std::optional<path_result> p = path(mgr, g, t, *sg, core::op_table::id, *sf);
        if (!p) break;
        note(depth, (n.kind == ctl::op::ef ? "EF: a path to " : "EU: a path through the left operand to ") + show(rg));
        run_after_first(depth, *p);
        formula(depth + 1, rg, p->states.back());
        return;
      }
      case ctl::op::eg: {
        const std::optional<code> sf = chk.sat(n.kids[0]);
        if (!sf) break;
        const code deadf = diagrams.meet(chk.the_model().dead, *sf);
        // A path through f to a dead f-state, if one is reachable that way.
        if (deadf != core::none) {
          const std::optional<path_result> p = path(mgr, g, t, deadf, core::op_table::id, *sf);
          if (p) {
            note(depth, "EG: a path through " + show(n.kids[0]) + " to a deadlock");
            run_after_first(depth, *p);
            note(depth, "deadlock: the path ends here");
            return;
          }
        }
        const std::optional<code> hull = chk.eg_hull(n.kids[0]);
        if (!hull || *hull == core::none) break;
        const std::optional<path_result> p = path(mgr, g, t, *hull, core::op_table::id, *sf);
        if (!p) break;
        note(depth, "EG: a path through " + show(n.kids[0]) + " to a cycle");
        run_after_first(depth, *p);
        lasso(depth, p->states.back(), *hull);
        return;
      }
      case ctl::op::ew: {
        const node_id eu = F.binary(ctl::op::eu, n.kids[0], n.kids[1]);
        const std::optional<code> su = chk.sat(eu);
        if (su && diagrams.meet(*su, t) != core::none) formula(depth, eu, t);
        else formula(depth, F.unary(ctl::op::eg, n.kids[0]), t);
        return;
      }
      default:
        note(depth, "holds on all paths (exhaustive): " + show(f));
        return;
    }
    note(depth, "no witness could be built for " + show(f));
  }

  /// Explain a set expression at the chosen state \p t of its set.
  void set(std::size_t depth, set_id s, code t) {
    ctl::forward& fw = chk.form();
    const ctl::set_expr e = fw.set(s);
    switch (e.kind) {
      case ctl::set_op::init:
        note(depth, "initial state");
        state(depth, t);
        return;
      case ctl::set_op::filter:
        set(depth, e.arg, t);
        note(depth, "holds: " + show(e.f));
        return;
      case ctl::set_op::ey: {
        const std::optional<code> r = chk.eval(e.arg);
        if (!r) break;
        // A predecessor of t inside [r]: through the inverted events when we
        // have them, else by trying every event forward.
        for (std::size_t j = 0; j < g.events.size(); ++j) {
          code src = core::none;
          if (j < g.preds.size()) {
            src = diagrams.meet(diagrams.apply_local(g.preds[j], t), *r);
          } else if (diagrams.meet(diagrams.apply_local(g.events[j], *r), t) != core::none) {
            src = *r;  // imprecise without a converse: the layer, not one state
          }
          if (src == core::none) continue;
          const code s0 = g.one_state(src);
          set(depth, e.arg, s0);
          event(depth, j);
          state(depth, t);
          return;
        }
        break;
      }
      case ctl::set_op::fwdu:
      case ctl::set_op::fwdg: {
        const std::optional<code> r = chk.eval(e.arg);
        const std::optional<code> sq = chk.sat(e.f);
        if (!r || !sq) break;
        code from = *r;
        if (e.kind == ctl::set_op::fwdg) from = diagrams.meet(from, *sq);
        const std::optional<path_result> p = path(mgr, g, from, t, core::op_table::id, *sq);
        if (!p) break;
        set(depth, e.arg, p->states.front());
        note(depth, "through " + show(e.f) + ":");
        run_after_first(depth, *p);
        if (e.kind == ctl::set_op::fwdg) {
          if (diagrams.meet(chk.the_model().dead, t) != core::none) {
            note(depth, "deadlock: the path ends here");
          } else {
            const std::optional<code> hull = chk.eg_hull(e.f);
            if (hull && *hull != core::none) lasso(depth, t, *hull);
            else note(depth, "a cycle exists here (no inverted events to trace it)");
          }
        }
        return;
      }
      case ctl::set_op::restrict_:
        set(depth, e.arg, t);
        formula(depth + 1, e.f, t);
        return;
    }
    note(depth, "no witness could be built for this set");
  }
};

}  // namespace

std::vector<witness_line> witness(
    core::manager& mgr, ctl::checker& chk, const graph& g,
    const ctl::forward_form& form,
    const std::function<std::string(std::uint32_t)>& atom_name) {
  builder b{mgr, chk, g, mgr.diagrams(), atom_name, {}};
  const std::optional<ctl::q_id> leaf = chk.answering_leaf(form);
  if (!leaf) {
    b.note(0, form.negated ? "the property holds on every path: no counterexample"
                           : "no path witnesses the property: it is false by exhaustion");
    return b.lines;
  }
  const set_id s = chk.form().q(*leaf).set;
  const std::optional<code> v = chk.eval(s);
  if (!v || *v == core::none) {
    b.note(0, "the answering set could not be rebuilt");
    return b.lines;
  }
  // The end state: for `filter(fwdu(r, q), f)` the one a shortest run from
  // [r] through q reaches first (shortest overall for that shape); else the
  // first state of the set.
  code end = core::none;
  const ctl::set_expr e = chk.form().set(s);
  if (e.kind == ctl::set_op::filter) {
    const ctl::set_expr inner = chk.form().set(e.arg);
    if (inner.kind == ctl::set_op::fwdu) {
      const std::optional<code> r = chk.eval(inner.arg);
      const std::optional<code> sq = chk.sat(inner.f);
      if (r && sq) {
        const std::optional<path_result> p = path(mgr, g, *r, *v, core::op_table::id, *sq);
        if (p) end = p->states.back();
      }
    }
  }
  if (end == core::none) end = g.one_state(*v);
  b.note(0, form.negated ? "counterexample" : "witness");
  b.set(0, s, end);
  return b.lines;
}

}  // namespace hsc::trace
