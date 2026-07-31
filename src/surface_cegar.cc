/// \file surface_cegar.cc
/// \brief The cegar bridge: separable-fragment checks, per-leaf letter
/// induction by domain enumeration, and the property-leaf monitor.
///
/// Local actions are evaluated with the `xpl` evaluator against a
/// one-position-varied seed word — sound because every accepted guard
/// atom and rhs has single-leaf support. Any ⊥ met during enumeration
/// refuses the model (the explicit engine would error on it at fire
/// time; parity O6 demands the same stance here).

#include "hsc/surface/cegar_build.hh"

#include <algorithm>
#include <map>
#include <sstream>

#include "hsc/surface/translate.hh"
#include "hsc/xpl/interpret/eval.hh"

namespace hsc::surface {

namespace {

[[noreturn]] void refuse(int line, const std::string& msg) {
  throw translate_error(line, "cegar: " + msg);
}

/// Arrays are outside the fragment; `at` anywhere in a datum refuses.
void refuse_at(const datum& d, const std::string& where) {
  if (d.is_atom()) return;
  if (d.head() == "at") refuse(d.line(), "array access in " + where);
  for (const datum& k : d.items()) refuse_at(k, where);
}

/// One event's separable compile: guard atoms and folded actions,
/// grouped by leaf position.
struct local_event {
  std::string name;
  int line = 0;
  bool dead = false;  ///< a constant guard atom folded to false
  std::map<std::uint32_t, std::vector<lia::bexpr>> guards;
  /// Per leaf, the action rhs sequence in clause order (`:=` desugared).
  std::map<std::uint32_t, std::vector<lia::iexpr>> acts;
};

local_event read_event(const xpl_source& src, lia::expr_factory& ex,
                       const expr_reader& reader, const name_scope& scope,
                       const std::vector<std::int32_t>& seed) {
  local_event ev;
  ev.name = src.name;
  ev.line = src.line;
  for (const datum& cl : src.clauses) {
    if (!cl.is_list() || cl.items().empty()) {
      refuse(cl.line(), "event " + ev.name + ": malformed clause");
    }
    if (cl.head() == "when") {
      for (std::size_t i = 1; i < cl.items().size(); ++i) {
        const datum& atom = cl.items()[i];
        refuse_at(atom, "event " + ev.name + " guard");
        const lia::bexpr b = reader.read_bool(atom);
        const auto sup = ex.support_bool(b);
        if (sup.size() > 1) {
          refuse(atom.line(), "event " + ev.name +
                                  ": guard atom crosses leaves "
                                  "(non-separable; deferred)");
        }
        if (sup.empty()) {  // a constant: fold once against the seed
          const xpl::state_view env{seed.data(), seed.size()};
          xpl::evaluator e(ex, env);
          const auto t = e.guard(b);
          if (t == lia::expr_factory::truth::undef) {
            refuse(atom.line(),
                   "event " + ev.name + ": guard is bottom (" + e.cause() + ")");
          }
          if (t == lia::expr_factory::truth::no) ev.dead = true;
          continue;
        }
        ev.guards[sup[0]].push_back(b);
      }
    } else if (cl.head() == "do") {
      std::vector<std::uint32_t> written_here;
      for (std::size_t i = 1; i < cl.items().size(); ++i) {
        const datum& act = cl.items()[i];
        if (!act.is_list() || act.items().size() != 3 ||
            (act.head() != ":=" && act.head() != "+=" && act.head() != "-=")) {
          refuse(act.line(), "event " + ev.name +
                                 ": only :=, +=, -= actions are in the "
                                 "fragment (havoc and arrays deferred)");
        }
        const datum& lhs = act.items()[1];
        if (!lhs.is_atom()) {
          refuse(lhs.line(),
                 "event " + ev.name + ": array target (deferred)");
        }
        const auto pos = scope.position(lhs.text());
        if (!pos) refuse(lhs.line(), "unknown leaf '" + lhs.text() + "'");
        if (std::find(written_here.begin(), written_here.end(), *pos) !=
            written_here.end()) {
          refuse(act.line(), "event " + ev.name + ": leaf '" + lhs.text() +
                                 "' written twice in one do clause");
        }
        written_here.push_back(*pos);
        refuse_at(act.items()[2], "event " + ev.name + " action");
        lia::iexpr rhs = reader.read_int(act.items()[2]);
        if (act.head() != ":=") {
          const lia::iexpr self = ex.variable(*pos);
          rhs = act.head() == "+=" ? ex.add(self, rhs) : ex.sub(self, rhs);
        }
        for (std::uint32_t q : ex.support(rhs)) {
          if (q != *pos) {
            refuse(act.line(), "event " + ev.name + ": action on '" +
                                   lhs.text() +
                                   "' reads another leaf "
                                   "(non-separable; deferred)");
          }
        }
        ev.acts[*pos].push_back(rhs);
      }
    } else {
      refuse(cl.line(), "event " + ev.name + ": clause '" + cl.head() +
                            "' is outside the fragment (alt/seq/havoc "
                            "events deferred)");
    }
  }
  return ev;
}

/// The local-action graph of one event on one leaf, in state space:
/// graph[s] = successor state, -1 undefined. nullopt when the event
/// does not touch the leaf.
std::vector<std::int32_t> local_graph(
    const local_event& ev, std::uint32_t p, const cegar::lts& leaf,
    const std::vector<std::int32_t>& state_of, std::int32_t lo,
    std::int32_t hi, lia::expr_factory& ex,
    const std::vector<std::int32_t>& seed) {
  std::vector<std::int32_t> g(static_cast<std::size_t>(leaf.n_states), -1);
  std::vector<std::int32_t> env = seed;
  const auto* guards = ev.guards.contains(p) ? &ev.guards.at(p) : nullptr;
  const auto* acts = ev.acts.contains(p) ? &ev.acts.at(p) : nullptr;
  for (std::int32_t s = 0; s < leaf.n_states; ++s) {
    std::int32_t v = leaf.value_of[static_cast<std::size_t>(s)];
    env[p] = v;
    xpl::evaluator e(ex, xpl::state_view{env.data(), env.size()});
    bool on = true;
    if (guards) {
      for (const lia::bexpr b : *guards) {
        const auto t = e.guard(b);
        if (t == lia::expr_factory::truth::undef) {
          refuse(ev.line, "event " + ev.name + ": guard is bottom (" +
                              e.cause() + ")");
        }
        if (t == lia::expr_factory::truth::no) {
          on = false;
          break;
        }
      }
    }
    if (!on) continue;
    if (acts) {
      for (const lia::iexpr rhs : *acts) {
        env[p] = v;
        xpl::evaluator step(ex, xpl::state_view{env.data(), env.size()});
        try {
          v = static_cast<std::int32_t>(step.strict_int(rhs));
        } catch (const xpl::eval_error& err) {
          refuse(ev.line, "event " + ev.name + ": action is bottom (" +
                              std::string(err.what()) + ")");
        }
      }
      if (v < lo || v >= hi) {
        refuse(ev.line, "event " + ev.name + " drives a leaf to " +
                            std::to_string(v) + ", outside its bound [" +
                            std::to_string(lo) + "," + std::to_string(hi) +
                            ")");
      }
    }
    g[static_cast<std::size_t>(s)] = state_of[static_cast<std::size_t>(v - lo)];
  }
  return g;
}

}  // namespace

cegar_bridge build_cegar_model(const spec& s, lia::expr_factory& ex,
                               const expr_reader& reader,
                               std::span<const datum> atoms) {
  cegar_bridge out;
  cegar::model& m = out.model;
  const std::size_t arity = s.order().size();
  if (arity == 0) refuse(0, "the spec declares no leaves");

  // One seed exactly (alt/havoc inits are deferred).
  std::vector<xpl::word> seeds = s.seeds(ex);
  if (seeds.size() != 1) {
    refuse(0, "the model has " + std::to_string(seeds.size()) +
                  " initial states; the fragment wants one");
  }
  out.seed.assign(seeds[0].begin(), seeds[0].end());

  // Leaves: bounded domains, seed renumbered to state 0.
  m.leaf_names = s.order();
  std::vector<std::int32_t> lo_of(arity), hi_of(arity);
  std::vector<std::vector<std::int32_t>> state_of(arity);
  for (std::uint32_t p = 0; p < arity; ++p) {
    const auto b = s.bound(p);
    if (!b) {
      refuse(0, "leaf '" + m.leaf_names[p] +
                    "' has no declared bound; the finite instance "
                    "wants (leaf NAME LO HI)");
    }
    const auto [lo, hi] = *b;
    const std::int32_t sv = out.seed[p];
    if (sv < lo || sv >= hi) {
      refuse(0, "leaf '" + m.leaf_names[p] + "' starts at " +
                    std::to_string(sv) + ", outside its bound");
    }
    lo_of[p] = lo;
    hi_of[p] = hi;
    cegar::lts l;
    l.n_states = hi - lo;
    l.value_of.reserve(static_cast<std::size_t>(l.n_states));
    l.value_of.push_back(sv);
    for (std::int32_t v = lo; v < hi; ++v)
      if (v != sv) l.value_of.push_back(v);
    state_of[p].assign(static_cast<std::size_t>(hi - lo), -1);
    for (std::int32_t st = 0; st < l.n_states; ++st)
      state_of[p][static_cast<std::size_t>(
          l.value_of[static_cast<std::size_t>(st)] - lo)] = st;
    m.leaves.push_back(std::move(l));
  }

  // Property atoms: single-leaf support each; the union names the
  // property leaves.
  std::vector<lia::bexpr> batoms;
  {
    std::ostringstream txt;
    for (std::size_t i = 0; i < atoms.size(); ++i) {
      refuse_at(atoms[i], "property atom");
      const lia::bexpr b = reader.read_bool(atoms[i]);
      const auto sup = ex.support_bool(b);
      if (sup.size() > 1) {
        refuse(atoms[i].line(),
               "property atom crosses leaves (non-separable; deferred)");
      }
      for (std::uint32_t q : sup)
        m.prop_leaves.push_back(static_cast<std::int32_t>(q));
      batoms.push_back(b);
      if (i) txt << ' ';
      write(txt, atoms[i]);
    }
    m.property_text = txt.str();
  }
  std::sort(m.prop_leaves.begin(), m.prop_leaves.end());
  m.prop_leaves.erase(
      std::unique(m.prop_leaves.begin(), m.prop_leaves.end()),
      m.prop_leaves.end());

  // Events: separable compile, then per-leaf graphs, letters deferred
  // until every graph is known (canonical numbering = lex order).
  std::vector<std::map<std::vector<std::int32_t>, cegar::letter>> letters(
      arity);
  struct ev_graphs {
    std::string name;
    std::vector<std::pair<std::uint32_t, std::vector<std::int32_t>>> per_leaf;
  };
  std::vector<ev_graphs> compiled;
  for (const xpl_source& src : s.events()) {
    const local_event ev = read_event(src, ex, reader, s, out.seed);
    if (ev.dead) continue;  // a false guard: the zero term, honestly dead
    ev_graphs eg;
    eg.name = ev.name;
    std::vector<std::uint32_t> touched;
    for (const auto& [p, _] : ev.guards) touched.push_back(p);
    for (const auto& [p, _] : ev.acts) touched.push_back(p);
    std::sort(touched.begin(), touched.end());
    touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
    for (std::uint32_t p : touched) {
      auto g = local_graph(ev, p, m.leaves[p], state_of[p], lo_of[p],
                           hi_of[p], ex, out.seed);
      letters[p].try_emplace(g, 0);  // rank assigned after the sweep
      eg.per_leaf.emplace_back(p, std::move(g));
    }
    compiled.push_back(std::move(eg));
  }
  for (std::uint32_t p = 0; p < arity; ++p) {
    cegar::letter next = 0;
    for (auto& [g, rank] : letters[p]) rank = next++;  // std::map: lex order
    cegar::lts& l = m.leaves[p];
    l.n_letters = static_cast<std::int32_t>(letters[p].size());
    l.delta.assign(
        static_cast<std::size_t>(l.n_states) * l.n_letters, -1);
    for (const auto& [g, rank] : letters[p])
      for (std::int32_t st = 0; st < l.n_states; ++st)
        l.delta[static_cast<std::size_t>(st) * l.n_letters + rank] =
            g[static_cast<std::size_t>(st)];
  }
  for (const ev_graphs& eg : compiled) {
    cegar::event e;
    e.name = eg.name;
    for (const auto& [p, g] : eg.per_leaf)
      e.support.emplace_back(static_cast<std::int32_t>(p),
                             letters[p].at(g));
    m.events.push_back(std::move(e));
  }

  // The monitor: exact sub-product of the property leaves. States are
  // reachable tuples of local states; values recorded for emission.
  {
    const auto& P = m.prop_leaves;
    std::map<std::vector<std::int32_t>, std::int32_t> id;
    std::vector<std::vector<std::int32_t>> tuples;
    std::vector<std::int32_t> init(P.size(), 0);
    id.emplace(init, 0);
    tuples.push_back(init);
    std::vector<std::int32_t> delta;  // row-appended alongside the BFS
    const std::int32_t ne = static_cast<std::int32_t>(m.events.size());
    for (std::size_t head = 0; head < tuples.size(); ++head) {
      const auto cur = tuples[head];
      for (std::int32_t e = 0; e < ne; ++e) {
        std::vector<std::int32_t> nxt = cur;
        bool blocked = false;
        for (std::size_t k = 0; k < P.size(); ++k) {
          const auto a = m.events[e].letter_for(P[k]);
          if (!a) continue;
          const std::int32_t q = m.leaves[P[k]].step(cur[k], *a);
          if (q < 0) {
            blocked = true;
            break;
          }
          nxt[k] = q;
        }
        if (blocked) {
          delta.push_back(-1);
          continue;
        }
        auto [it, fresh] =
            id.try_emplace(nxt, static_cast<std::int32_t>(tuples.size()));
        if (fresh) tuples.push_back(nxt);
        delta.push_back(it->second);
      }
    }
    m.mon.n_states = static_cast<std::int32_t>(tuples.size());
    m.mon.n_events = ne;
    m.mon.delta = std::move(delta);
    m.mon.bad.assign(static_cast<std::size_t>(m.mon.n_states), false);
    std::vector<std::int32_t> env = out.seed;
    for (std::int32_t t = 0; t < m.mon.n_states; ++t) {
      std::vector<std::int32_t> vals;
      for (std::size_t k = 0; k < P.size(); ++k) {
        const cegar::lts& l = m.leaves[P[k]];
        const std::int32_t v =
            l.value_of[static_cast<std::size_t>(tuples[t][k])];
        env[P[k]] = v;
        vals.push_back(v);
      }
      xpl::evaluator e(ex, xpl::state_view{env.data(), env.size()});
      bool bad = true;
      for (const lia::bexpr b : batoms) {
        const auto tr = e.guard(b);
        if (tr == lia::expr_factory::truth::undef) {
          refuse(0, "property atom is bottom (" + e.cause() + ")");
        }
        if (tr == lia::expr_factory::truth::no) {
          bad = false;
          break;
        }
      }
      m.mon.bad[static_cast<std::size_t>(t)] = bad;
      m.monitor_values.push_back(std::move(vals));
    }
  }
  return out;
}

}  // namespace hsc::surface
