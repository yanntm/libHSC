/// \file checker.cc
/// \brief Evaluation of set expressions and backward `Sat` over a model
/// (`hsc/ctl/checker.hh`, `algorithm.md` §3–§7).
#include "hsc/ctl/checker.hh"

#include <cstdlib>
#include <string>

#include "hsc/core/diagram.hh"
#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"
#include "hsc/util/hash.hh"

namespace hsc::ctl {

const char* name(verdict v) noexcept {
  switch (v) {
    case verdict::yes: return "TRUE";
    case verdict::no: return "FALSE";
    case verdict::unknown: return "UNKNOWN";
  }
  return "?";
}

std::size_t checker::closure_hash::operator()(
    const closure_key& k) const noexcept {
  std::size_t seed = util::hash_value(static_cast<std::uint8_t>(
      (k.backward ? 1 : 0) | (k.before ? 2 : 0)));
  util::hash_combine(seed, k.sel);
  return seed;
}

checker::checker(core::manager& mgr, const model& m, forward& fw)
    : mgr_(mgr), m_(m), fw_(fw), f_(fw.forms()) {}

// --- the model's operations -------------------------------------------------

checker::code checker::step(bool backward, code s) {
  const std::span<const code> evs = events(backward);
  if (evs.empty() || s == core::none) return core::none;
  code& stp = backward ? pred_step_ : next_step_;
  bool& built = backward ? pred_built_ : next_built_;
  if (!built) {
    stp = core::sum_at(mgr_, m_.sort, evs);
    built = true;
  }
  return mgr_.diagrams().apply_local(stp, s);
}

bool checker::existential_enabled() {
  static const bool on = [] {
    const char* e = std::getenv("HSC_CTL_EXIST");
    return e == nullptr || std::string(e) != "0";
  }();
  return on;
}

checker::code checker::closure(bool backward, code sel, bool before) {
  const closure_key key{backward, before, sel};
  if (const auto it = closure_memo_.find(key); it != closure_memo_.end())
    return it->second;
  std::vector<code> filtered;
  filtered.reserve(events(backward).size());
  core::op_table& ops = mgr_.operations();
  for (const code t : events(backward)) {
    // compose(after, before): the filter before the step is `t ∘ sel`.
    filtered.push_back(before ? ops.compose(t, sel) : ops.compose(sel, t));
  }
  const code c = core::saturate(mgr_, m_.sort, filtered);
  closure_memo_.emplace(key, c);
  return c;
}

checker::code checker::filter(code s, node_id f) {
  if (s == core::none) return core::none;
  const fnode& n = f_[f];
  if (n.kind == op::tru) return s;
  if (n.kind == op::fls) return core::none;
  auto it = sel_memo_.find(f);
  if (it == sel_memo_.end()) it = sel_memo_.emplace(f, m_.selector(f)).first;
  return mgr_.diagrams().apply_local(it->second, s);
}

std::optional<checker::code> checker::restrict(code s, node_id q) {
  if (f_.is_state(q)) return filter(s, q);
  const std::optional<code> sq = sat(q);
  if (!sq) return std::nullopt;
  return mgr_.diagrams().meet(s, *sq);
}

std::optional<checker::code> checker::constrained_lfp(bool backward,
                                                      code from, node_id q,
                                                      bool before) {
  core::diagram_engine& diagrams = mgr_.diagrams();
  if (from == core::none) return core::none;
  if (events(backward).empty()) return from;
  const fnode& n = f_[q];
  if (n.kind == op::fls) return from;  // no step passes the filter
  if (f_.is_state(q)) {
    code sel = core::op_table::id;
    if (n.kind != op::tru) {
      auto it = sel_memo_.find(q);
      if (it == sel_memo_.end())
        it = sel_memo_.emplace(q, m_.selector(q)).first;
      sel = it->second;
    }
    return diagrams.apply_local(closure(backward, sel, before), from);
  }
  // A temporal constraint: its Sat as data, met after (or before) each
  // step, breadth-first.
  const std::optional<code> s = sat(q);
  if (!s) return std::nullopt;
  code x = from;
  for (;;) {
    mgr_.check_interrupt();
    const code img = before ? step(backward, diagrams.meet(x, *s))
                            : diagrams.meet(step(backward, x), *s);
    const code y = diagrams.join(x, img);
    if (y == x) return x;
    x = y;
  }
}

bool checker::otf_enabled() {
  static const bool on = [] {
    const char* e = std::getenv("HSC_CTL_OTF");
    return e == nullptr || std::string(e) != "0";
  }();
  return on;
}

std::optional<bool> checker::exists_through(set_id r0, node_id q, code sel) {
  // Does a state satisfying the selector lie in lfp Z[r0 ∨ EY(Z ∧ q)]?
  // Breadth-first from r0 through q-states, the goal tested on each new
  // frontier: the search stops at the first hit instead of closing the set.
  core::diagram_engine& diagrams = mgr_.diagrams();
  const std::optional<code> a0 = eval(r0);
  if (!a0) return std::nullopt;
  if (*a0 == core::none) return false;
  const std::optional<code> sq = sat(q);
  if (!sq) return std::nullopt;
  code seen = *a0;
  code frontier = *a0;
  for (;;) {
    mgr_.check_interrupt();
    if (diagrams.has_image(sel, frontier) != core::none) return true;
    const code img = step(false, diagrams.meet(frontier, *sq));
    frontier = diagrams.minus(img, seen);
    if (frontier == core::none) return false;
    seen = diagrams.join(seen, frontier);
  }
}

bool checker::has_cycles() {
  if (!cycles_) {
    if (m_.next_events.empty()) {
      cycles_ = false;
    } else {
      step(false, m_.reach);  // builds the step term
      const code g = mgr_.operations().gfp(next_step_);
      const code w = existential_enabled()
                         ? mgr_.diagrams().has_image(g, m_.reach)
                         : mgr_.diagrams().apply_local(g, m_.reach);
      cycles_ = w != core::none;
    }
  }
  return *cycles_;
}

std::optional<bool> checker::nonempty(set_id s) {
  if (!existential_enabled()) {
    const std::optional<code> v = eval(s);
    if (!v) return std::nullopt;
    return *v != core::none;
  }
  const set_expr e = fw_.set(s);
  core::diagram_engine& diagrams = mgr_.diagrams();
  switch (e.kind) {
    case set_op::init:
      return m_.init != core::none;
    case set_op::filter: {
      const fnode& n = f_[e.f];
      if (n.kind == op::fls) return false;
      const set_expr& below = fw_.set(e.arg);
      if (n.kind != op::tru && below.kind == set_op::fwdu &&
          !f_.is_state(below.f) && otf_enabled()) {
        // A closure constrained by a temporal formula is breadth-first
        // anyway: search it on the fly for the goal instead of closing it.
        auto it = sel_memo_.find(e.f);
        if (it == sel_memo_.end())
          it = sel_memo_.emplace(e.f, m_.selector(e.f)).first;
        return exists_through(below.arg, below.f, it->second);
      }
      const std::optional<code> a = eval(e.arg);
      if (!a) return std::nullopt;
      if (*a == core::none) return false;
      if (n.kind == op::tru) return true;
      auto it = sel_memo_.find(e.f);
      if (it == sel_memo_.end())
        it = sel_memo_.emplace(e.f, m_.selector(e.f)).first;
      return diagrams.has_image(it->second, *a) != core::none;
    }
    case set_op::ey: {
      const std::optional<code> a = eval(e.arg);
      if (!a) return std::nullopt;
      if (*a == core::none || m_.next_events.empty()) return false;
      step(false, m_.reach);  // builds the step term
      return diagrams.has_image(next_step_, *a) != core::none;
    }
    case set_op::fwdu: {
      // The closure contains its seed.
      const std::optional<code> a = eval(e.arg);
      if (!a) return std::nullopt;
      return *a != core::none;
    }
    case set_op::fwdg: {
      const std::optional<code> a = eval(e.arg);
      if (!a) return std::nullopt;
      const std::optional<code> seed = restrict(*a, e.f);
      if (!seed) return std::nullopt;
      if (*seed == core::none) return false;
      const std::optional<code> rq =
          constrained_lfp(false, *seed, e.f, /*before=*/true);
      if (!rq) return std::nullopt;
      const std::optional<code> reach_q = restrict(*rq, e.f);
      if (!reach_q) return std::nullopt;
      if (*reach_q == core::none) return false;
      if (diagrams.meet(m_.dead, *reach_q) != core::none) return true;
      if (m_.next_events.empty() || !has_cycles()) return false;
      step(false, *reach_q);
      return diagrams.has_image(mgr_.operations().gfp(next_step_), *reach_q) !=
             core::none;
    }
    case set_op::restrict_: {
      const std::optional<code> v = eval(s);
      if (!v) return std::nullopt;
      return *v != core::none;
    }
  }
  return std::nullopt;
}

// --- set expressions --------------------------------------------------------

std::optional<checker::code> checker::eval(set_id s) {
  if (const auto it = set_memo_.find(s); it != set_memo_.end())
    return it->second;
  const set_expr e = fw_.set(s);
  core::diagram_engine& diagrams = mgr_.diagrams();
  std::optional<code> r;
  switch (e.kind) {
    case set_op::init:
      r = m_.init;
      break;
    case set_op::filter: {
      const std::optional<code> a = eval(e.arg);
      if (a) r = filter(*a, e.f);
      break;
    }
    case set_op::ey: {
      const std::optional<code> a = eval(e.arg);
      if (a) r = step(false, *a);
      break;
    }
    case set_op::fwdu: {
      const std::optional<code> a = eval(e.arg);
      if (a) r = constrained_lfp(false, *a, e.f, /*before=*/true);
      break;
    }
    case set_op::fwdg: {
      // reach_q = filter(fwdu(filter(r, q), q), q);
      // fwdg = gfp(next)·reach_q ∪ (dead ∩ reach_q)
      const std::optional<code> a = eval(e.arg);
      if (!a) break;
      const std::optional<code> seed = restrict(*a, e.f);
      if (!seed) break;
      const std::optional<code> rq =
          constrained_lfp(false, *seed, e.f, /*before=*/true);
      if (!rq) break;
      const std::optional<code> reach_q = restrict(*rq, e.f);
      if (!reach_q) break;
      code res = diagrams.meet(m_.dead, *reach_q);
      if (*reach_q != core::none && !m_.next_events.empty() && has_cycles()) {
        step(false, *reach_q);  // ensures next_step_
        res = diagrams.join(
            res, diagrams.apply_local(mgr_.operations().gfp(next_step_),
                                      *reach_q));
      }
      r = res;
      break;
    }
    case set_op::restrict_: {
      const std::optional<code> a = eval(e.arg);
      if (!a) break;
      const std::optional<code> sf = sat(e.f);
      if (sf) r = diagrams.meet(*a, *sf);
      break;
    }
  }
  set_memo_.emplace(s, r);
  return r;
}

// --- backward Sat -----------------------------------------------------------

std::optional<checker::code> checker::sat_eu(node_id f, node_id g) {
  const std::optional<code> sg = sat(g);
  if (!sg) return std::nullopt;
  if (events(true).empty()) return std::nullopt;
  return constrained_lfp(true, *sg, f, /*before=*/false);
}

std::optional<checker::code> checker::sat_eg(node_id f) {
  const std::optional<code> sf = sat(f);
  if (!sf) return std::nullopt;
  if (events(true).empty()) return std::nullopt;
  core::diagram_engine& diagrams = mgr_.diagrams();
  // D ∪ lfp(sel_f ∘ pred)·D ∪ gfp(pred)·Sat(f), D = dead ∩ Sat(f)
  const code d = diagrams.meet(m_.dead, *sf);
  std::optional<code> res = constrained_lfp(true, d, f, /*before=*/false);
  if (!res) return std::nullopt;
  if (*sf != core::none && has_cycles()) {
    step(true, *sf);  // ensures pred_step_
    const code g = mgr_.operations().gfp(pred_step_);
    // A witness first: no cycle in Sat f means no hull to pay for.
    if (!existential_enabled() || diagrams.has_image(g, *sf) != core::none) {
      *res = diagrams.join(*res, diagrams.apply_local(g, *sf));
    }
  }
  return res;
}

std::optional<checker::code> checker::sat(node_id f) {
  if (const auto it = sat_memo_.find(f); it != sat_memo_.end())
    return it->second;
  const fnode n = f_[f];
  core::diagram_engine& diagrams = mgr_.diagrams();
  const code R = m_.reach;
  std::optional<code> r;
  auto neg = [&](std::optional<code> x) -> std::optional<code> {
    if (!x) return std::nullopt;
    return diagrams.minus(R, *x);
  };
  if (f_.is_state(f)) {
    r = filter(R, f);
  } else {
    switch (n.kind) {
      case op::not_:
        r = neg(sat(n.kids[0]));
        break;
      case op::and_:
      case op::or_: {
        code acc = n.kind == op::and_ ? R : core::none;
        bool ok = true;
        for (const node_id k : n.kids) {
          const std::optional<code> s = sat(k);
          if (!s) {
            ok = false;
            break;
          }
          acc = n.kind == op::and_ ? diagrams.meet(acc, *s)
                                   : diagrams.join(acc, *s);
        }
        if (ok) r = acc;
        break;
      }
      case op::ex: {
        const std::optional<code> s = sat(n.kids[0]);
        if (s && !events(true).empty()) r = step(true, *s);
        break;
      }
      case op::ax: {  // ¬EX¬f
        const std::optional<code> s = sat(f_.negate(n.kids[0]));
        if (s && !events(true).empty()) r = neg(step(true, *s));
        break;
      }
      case op::ef:
        r = sat_eu(f_.constant(true), n.kids[0]);
        break;
      case op::ag:  // ¬EF¬f
        r = neg(sat_eu(f_.constant(true), f_.negate(n.kids[0])));
        break;
      case op::eg:
        r = sat_eg(n.kids[0]);
        break;
      case op::af:  // ¬EG¬f
        r = neg(sat_eg(f_.negate(n.kids[0])));
        break;
      case op::eu:
        r = sat_eu(n.kids[0], n.kids[1]);
        break;
      case op::ew: {  // E[f U g] ∨ EG f
        const std::optional<code> u = sat_eu(n.kids[0], n.kids[1]);
        const std::optional<code> g = sat_eg(n.kids[0]);
        if (u && g) r = diagrams.join(*u, *g);
        break;
      }
      case op::au: {  // ¬(E[¬g U (¬f ∧ ¬g)] ∨ EG ¬g)
        const node_id nf = f_.negate(n.kids[0]);
        const node_id ng = f_.negate(n.kids[1]);
        const std::optional<code> u = sat_eu(ng, f_.conj(nf, ng));
        const std::optional<code> g = sat_eg(ng);
        if (u && g) r = neg(diagrams.join(*u, *g));
        break;
      }
      case op::aw: {  // ¬E[¬g U (¬f ∧ ¬g)]
        const node_id nf = f_.negate(n.kids[0]);
        const node_id ng = f_.negate(n.kids[1]);
        r = neg(sat_eu(ng, f_.conj(nf, ng)));
        break;
      }
      default:
        break;
    }
  }
  sat_memo_.emplace(f, r);
  return r;
}

// --- the verdict ------------------------------------------------------------

verdict checker::ask(q_id qi) {
  const question q = fw_.q(qi);
  switch (q.kind) {
    case q_op::never:
      return verdict::no;
    case q_op::nonempty: {
      const std::optional<bool> s = nonempty(q.set);
      if (!s) return verdict::unknown;
      return *s ? verdict::yes : verdict::no;
    }
    case q_op::any: {
      bool unknown = false;
      for (const q_id k : q.kids) {
        const verdict v = ask(k);
        if (v == verdict::yes) return verdict::yes;
        if (v == verdict::unknown) unknown = true;
      }
      return unknown ? verdict::unknown : verdict::no;
    }
  }
  return verdict::unknown;
}

verdict checker::check(const forward_form& form) {
  const verdict v = ask(form.root);
  if (!form.negated || v == verdict::unknown) return v;
  return v == verdict::yes ? verdict::no : verdict::yes;
}

}  // namespace hsc::ctl
