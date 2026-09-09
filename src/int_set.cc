#include "hsc/leaves/int_set.hh"

#include <algorithm>
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "hsc/util/errors.hh"

namespace hsc::leaves {

namespace {

/// The probe view: a set that has not been built.
struct int_set_view {
  std::span<const std::int32_t> values;

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(
        static_cast<std::uint32_t>(values.size()));
    util::hash_range(seed, values.begin(), values.end());
    return seed;
  }
  [[nodiscard]] bool equals(const int_set& s) const {
    return s.count == values.size() &&
           std::equal(values.begin(), values.end(), s.data());
  }
  [[nodiscard]] std::size_t extra_bytes() const {
    return values.size() * sizeof(std::int32_t);
  }
  int_set* construct(void* mem) const {
    auto* p = new (mem) int_set{static_cast<std::uint32_t>(values.size())};
    std::copy(values.begin(), values.end(),
              const_cast<std::int32_t*>(p->data()));
    return p;
  }
};

}  // namespace

core::code int_set_theory::of_sorted(std::span<const std::int32_t> values) {
  if (values.empty()) return core::none;  // absence, never a code
  return table_.get(int_set_view{values});
}

core::code int_set_theory::of(std::span<const std::int32_t> values) {
  scratch_.assign(values.begin(), values.end());
  std::ranges::sort(scratch_);
  const auto dup = std::ranges::unique(scratch_);
  scratch_.erase(dup.begin(), dup.end());
  return of_sorted(scratch_);
}

core::code int_set_theory::singleton(std::int32_t v) {
  return of_sorted(std::span<const std::int32_t>(&v, 1));
}

core::code int_set_theory::interval(std::int32_t lo, std::int32_t hi) {
  if (hi <= lo) return core::none;
  scratch_.clear();
  scratch_.reserve(static_cast<std::size_t>(hi - lo));
  for (std::int32_t v = lo; v < hi; ++v) scratch_.push_back(v);
  return of_sorted(scratch_);
}

std::span<const std::int32_t> int_set_theory::elements(core::code c) const {
  if (c == core::none) return {};
  return table_[c].elements();
}

core::code int_set_theory::keep(core::code set) {
  if (set == core::none) return core::none;  // a guard nothing satisfies
  return terms_.get(int_term{int_shape::primitive, int_action::keep,
                             int_guard::set, 0, set, 0});
}

core::code int_set_theory::assign(core::code set, std::int32_t value) {
  const int_guard g = set == core::none ? int_guard::none : int_guard::set;
  return terms_.get(
      int_term{int_shape::primitive, int_action::assign, g, value, set, 0});
}

core::code int_set_theory::shift(core::code set, std::int32_t delta) {
  if (delta == 0) return set == core::none ? core::code{0} : keep(set);
  const int_guard g = set == core::none ? int_guard::none : int_guard::set;
  return terms_.get(
      int_term{int_shape::primitive, int_action::shift, g, delta, set, 0});
}

core::code int_set_theory::keep_if(lia::bexpr g) {
  if (g == lia::btrue) return 0;  // keep everything: id
  return terms_.get(int_term{int_shape::primitive, int_action::keep,
                             int_guard::symbolic, 0, g, 0});
}

core::code int_set_theory::assign_if(lia::bexpr g, std::int32_t value) {
  if (g == lia::btrue) return assign(core::none, value);
  return terms_.get(int_term{int_shape::primitive, int_action::assign,
                             int_guard::symbolic, value, g, 0});
}

core::code int_set_theory::shift_if(lia::bexpr g, std::int32_t delta) {
  if (delta == 0) return keep_if(g);
  if (g == lia::btrue) return shift(core::none, delta);
  return terms_.get(int_term{int_shape::primitive, int_action::shift,
                             int_guard::symbolic, delta, g, 0});
}

core::code int_set_theory::apply_if(lia::bexpr g, lia::iexpr rhs,
                                    std::int32_t modulo) {
  // Fold to the cheaper forms when the expression is one of them.
  const bool is_node = (rhs & 1u) == 0;
  if (lia::expr_factory::is_const(rhs) ||
      (is_node && exprs_.kind(rhs) == lia::ikind::constant)) {
    std::int32_t v = exprs_.value(rhs);
    if (modulo != 0) v = static_cast<std::int32_t>(
        ((static_cast<std::int64_t>(v) % modulo) + modulo) % modulo);
    return assign_if(g, v);
  }
  if (rhs == lia::iundef) return keep_if(lia::bfalse);  // abort: the 0 term
  const lia::iexpr x = exprs_.variable(0);
  if (modulo == 0) {
    if (rhs == x) return keep_if(g);
    if (is_node && exprs_.kind(rhs) == lia::ikind::plus) {
      const auto& n = exprs_.node(rhs);
      if (n.count == 2) {  // x + k, operands sorted by code
        const lia::iexpr a = n.operands()[0];
        const lia::iexpr b = n.operands()[1];
        if (a == x && lia::expr_factory::is_const(b)) {
          return shift_if(g, exprs_.value(b));
        }
        if (b == x && lia::expr_factory::is_const(a)) {
          return shift_if(g, exprs_.value(a));
        }
      }
    }
  }
  const int_guard gk = g == lia::btrue ? int_guard::none : int_guard::symbolic;
  return terms_.get(int_term{int_shape::primitive, int_action::xform, gk,
                             modulo, gk == int_guard::none ? core::none : g,
                             rhs});
}

core::code int_set_theory::havoc_if(lia::bexpr g, std::int32_t lo,
                                    std::int32_t hi) {
  if (hi <= lo) return keep_if(lia::bfalse);  // no value to pick: the 0
  const int_guard gk = g == lia::btrue ? int_guard::none : int_guard::symbolic;
  return terms_.get(int_term{int_shape::primitive, int_action::havoc, gk, lo,
                             gk == int_guard::none ? core::none : g,
                             static_cast<core::code>(hi)});
}

core::code int_set_theory::choose(core::code guard, core::code set) {
  if (set == core::none) return keep_if(lia::bfalse);  // nothing to pick: 0
  const int_guard g = guard == core::none ? int_guard::none : int_guard::set;
  return terms_.get(
      int_term{int_shape::primitive, int_action::choose, g, 0, guard, set});
}

core::code int_set_theory::invert_local(core::code term, core::code domain) {
  if (term == 0) return 0;  // id is self-converse
  const core::code zero = keep_if(lia::bfalse);
  if (domain == core::none) return zero;
  const int_term t = terms_[term];  // copy: interning below may grow terms_
  if (t.shape == int_shape::sum) {
    return term_sum(invert_local(t.a, domain), invert_local(t.b, domain));
  }
  if (t.shape == int_shape::lfp) return term_lfp(invert_local(t.a, domain));

  // The guard, as the extensional set of domain values passing it.
  core::code passing = domain;
  switch (t.gkind) {
    case int_guard::none: break;
    case int_guard::set: passing = meet(domain, t.a); break;
    case int_guard::symbolic: passing = filter(domain, t.a); break;
  }
  if (passing == core::none) return zero;

  switch (t.action) {
    case int_action::keep:
      return keep(passing);
    case int_action::assign:
      // {(v, c) : g(v)} reversed: from c, any passing v
      return choose(singleton(t.arg), passing);
    case int_action::shift: {
      // {(v, v + delta) : g(v)} reversed: from v + delta back to v; the
      // guard is the passing set shifted.
      const auto from = elements(passing);
      std::vector<std::int32_t> out;
      out.reserve(from.size());
      for (const std::int32_t v : from) {
        std::int32_t nv;
        if (__builtin_add_overflow(v, t.arg, &nv)) {
          throw overflow_error("int32 overflow inverting a shift by " +
                               std::to_string(t.arg));
        }
        out.push_back(nv);
      }
      return shift(of_sorted(out), -t.arg);
    }
    case int_action::xform: {
      // {(v, e(v)) : g(v)} reversed: one choose per image value.
      std::map<std::int32_t, std::vector<std::int32_t>> back;
      for (const std::int32_t v : elements(passing)) {
        const std::int32_t env[] = {v};
        bool undef = false;
        std::int64_t r = exprs_.eval_int(t.b, env, undef);
        if (undef) continue;
        if (t.arg != 0) {
          r = ((r % t.arg) + t.arg) % t.arg;
        } else if (r < INT32_MIN || r > INT32_MAX) {
          throw overflow_error("int32 overflow inverting a transform");
        }
        back[static_cast<std::int32_t>(r)].push_back(v);
      }
      core::code acc = zero;
      for (auto& [u, vs] : back) {
        const core::code c = choose(singleton(u), of(vs));
        acc = acc == zero ? c : term_sum(acc, c);
      }
      return acc;
    }
    case int_action::havoc:
      // {(v, u) : g(v), u in [lo, hi)} reversed
      return choose(interval(t.arg, static_cast<std::int32_t>(t.b)), passing);
    case int_action::choose:
      // {(v, u) : g(v), u in S} reversed: from S, any passing v
      return choose(t.b, passing);
  }
  return zero;
}

namespace {
/// Guards of two kinds conjoined into one: symbolic with symbolic by `conj`,
/// set with set by `meet`, mixed by filtering the set. `none` guard = true.
struct guard_conj {
  int_guard kind = int_guard::none;
  core::code g = core::none;
};
}  // namespace

core::code int_set_theory::term_compose(core::code after, core::code before) {
  if (before == 0) return after;
  if (after == 0) return before;
  const int_term a = terms_[after];   // copies: interning below may grow terms_
  const int_term b = terms_[before];
  if (b.shape == int_shape::sum) {
    return term_sum(term_compose(after, b.a), term_compose(after, b.b));
  }
  if (a.shape == int_shape::sum) {
    return term_sum(term_compose(a.a, before), term_compose(a.b, before));
  }
  if (a.shape != int_shape::primitive || b.shape != int_shape::primitive) {
    throw unsupported_error("int_set: a closure does not compose");
  }
  const core::code zero = keep_if(lia::bfalse);
  const lia::iexpr x = exprs_.variable(0);

  // The second guard, read after the first action: a symbolic guard by
  // substitution, an extensional one by a set computation; `passing` is
  // the set the first action's values are drawn from when it chooses.
  guard_conj g2;  // g2 after act1, over the pre-value x
  core::code choose_from = core::none;  // for choose/havoc: S ∩ g2
  bool g2_value = false;  // for assign: whether the constant passes g2
  switch (b.action) {
    case int_action::keep:
      g2.kind = a.gkind;
      g2.g = a.a;
      break;
    case int_action::shift:
      if (a.gkind == int_guard::none) break;
      if (a.gkind == int_guard::symbolic) {
        g2.kind = int_guard::symbolic;
        g2.g = exprs_.subst_bool(a.a, 0, exprs_.add(x, exprs_.constant(b.arg)));
      } else {  // v passes iff v + d in the set: shift the set by -d
        std::vector<std::int32_t> out;
        for (const std::int32_t v : elements(a.a)) {
          std::int32_t nv;
          if (__builtin_sub_overflow(v, b.arg, &nv)) continue;
          out.push_back(nv);
        }
        g2.kind = int_guard::set;
        g2.g = of_sorted(out);
        if (g2.g == core::none) return zero;
      }
      break;
    case int_action::assign: {
      if (a.gkind == int_guard::none) g2_value = true;
      else if (a.gkind == int_guard::set) g2_value = meet(a.a, singleton(b.arg)) != core::none;
      else {
        const std::int32_t env[] = {b.arg};
        g2_value = exprs_.eval_bool(a.a, env) == lia::expr_factory::truth::yes;
      }
      if (!g2_value) return zero;
      break;
    }
    case int_action::choose:
    case int_action::havoc: {
      const core::code s0 = b.action == int_action::choose
                                ? b.b
                                : interval(b.arg, static_cast<std::int32_t>(b.b));
      choose_from = a.gkind == int_guard::none ? s0
                    : a.gkind == int_guard::set ? meet(s0, a.a)
                                                : filter(s0, a.a);
      if (choose_from == core::none) return zero;
      break;
    }
    case int_action::xform:
      if (b.arg != 0) throw unsupported_error("int_set: modulo under composition");
      if (a.gkind == int_guard::set) throw unsupported_error("int_set: set guard after a transform");
      if (a.gkind == int_guard::symbolic) {
        g2.kind = int_guard::symbolic;
        g2.g = exprs_.subst_bool(a.a, 0, b.b);
      }
      break;
  }

  // The composite guard: g1 ∧ g2-after-act1.
  guard_conj g{b.gkind, b.a};
  if (g2.kind != int_guard::none) {
    if (g.kind == int_guard::none) g = g2;
    else if (g.kind == int_guard::symbolic && g2.kind == int_guard::symbolic) g.g = exprs_.conj(g.g, g2.g);
    else if (g.kind == int_guard::set && g2.kind == int_guard::set) g.g = meet(g.g, g2.g);
    else if (g.kind == int_guard::set) g.g = filter(g.g, g2.g);
    else { g.kind = int_guard::set; g.g = filter(g2.g, g.g); }
    if (g.kind == int_guard::set && g.g == core::none) return zero;
    if (g.kind == int_guard::symbolic && g.g == lia::bfalse) return zero;
    if (g.kind == int_guard::symbolic && g.g == lia::btrue) g.kind = int_guard::none;
  }
  const auto prim = [&](int_action act, std::int32_t arg, core::code bb) {
    return terms_.get(int_term{int_shape::primitive, act, g.kind, arg,
                               g.kind == int_guard::none ? core::none : g.g, bb});
  };

  // The composite action act2 ∘ act1.
  switch (b.action) {
    case int_action::keep:
      return prim(a.action, a.arg, a.b);
    case int_action::shift:
      switch (a.action) {
        case int_action::keep: return prim(int_action::shift, b.arg, 0);
        case int_action::shift: {
          std::int32_t d;
          if (__builtin_add_overflow(a.arg, b.arg, &d)) throw overflow_error("shift composition");
          return d == 0 ? prim(int_action::keep, 0, 0) : prim(int_action::shift, d, 0);
        }
        case int_action::assign: return prim(int_action::assign, a.arg, 0);
        case int_action::choose: return prim(int_action::choose, 0, a.b);
        case int_action::havoc: return prim(int_action::havoc, a.arg, a.b);
        case int_action::xform:
          if (a.arg != 0) throw unsupported_error("int_set: modulo under composition");
          return prim(int_action::xform, 0, exprs_.subst(a.b, 0, exprs_.add(x, exprs_.constant(b.arg))));
      }
      break;
    case int_action::assign:
      switch (a.action) {
        case int_action::keep: return prim(int_action::assign, b.arg, 0);
        case int_action::shift: {
          std::int32_t c;
          if (__builtin_add_overflow(b.arg, a.arg, &c)) throw overflow_error("assign then shift");
          return prim(int_action::assign, c, 0);
        }
        case int_action::assign: return prim(int_action::assign, a.arg, 0);
        case int_action::choose: return prim(int_action::choose, 0, a.b);
        case int_action::havoc: return prim(int_action::havoc, a.arg, a.b);
        case int_action::xform: {
          const std::int32_t env[] = {b.arg};
          bool undef = false;
          std::int64_t r = exprs_.eval_int(a.b, env, undef);
          if (undef) return zero;
          if (a.arg != 0) r = ((r % a.arg) + a.arg) % a.arg;
          if (r < INT32_MIN || r > INT32_MAX) throw overflow_error("assign then transform");
          return prim(int_action::assign, static_cast<std::int32_t>(r), 0);
        }
      }
      break;
    case int_action::choose:
    case int_action::havoc:
      switch (a.action) {
        case int_action::keep: return prim(int_action::choose, 0, choose_from);
        case int_action::shift: {
          std::vector<std::int32_t> out;
          for (const std::int32_t v : elements(choose_from)) {
            std::int32_t nv;
            if (__builtin_add_overflow(v, a.arg, &nv)) throw overflow_error("choose then shift");
            out.push_back(nv);
          }
          return prim(int_action::choose, 0, of_sorted(out));
        }
        case int_action::assign: return prim(int_action::assign, a.arg, 0);
        case int_action::choose: return prim(int_action::choose, 0, a.b);
        case int_action::havoc: return prim(int_action::havoc, a.arg, a.b);
        case int_action::xform: {
          std::vector<std::int32_t> out;
          for (const std::int32_t v : elements(choose_from)) {
            const std::int32_t env[] = {v};
            bool undef = false;
            std::int64_t r = exprs_.eval_int(a.b, env, undef);
            if (undef) continue;
            if (a.arg != 0) r = ((r % a.arg) + a.arg) % a.arg;
            if (r < INT32_MIN || r > INT32_MAX) throw overflow_error("choose then transform");
            out.push_back(static_cast<std::int32_t>(r));
          }
          return prim(int_action::choose, 0, of(out));
        }
      }
      break;
    case int_action::xform:
      switch (a.action) {
        case int_action::keep: return prim(int_action::xform, 0, b.b);
        case int_action::shift: return prim(int_action::xform, 0, exprs_.add(b.b, exprs_.constant(a.arg)));
        case int_action::assign: return prim(int_action::assign, a.arg, 0);
        case int_action::choose: return prim(int_action::choose, 0, a.b);
        case int_action::havoc: return prim(int_action::havoc, a.arg, a.b);
        case int_action::xform:
          if (a.arg != 0) throw unsupported_error("int_set: modulo under composition");
          return prim(int_action::xform, 0, exprs_.subst(a.b, 0, b.b));
      }
      break;
  }
  throw unsupported_error("int_set: composition not spelled");
}

core::code int_set_theory::filter(core::code set, lia::bexpr g) {
  if (set == core::none || g == lia::bfalse) return core::none;
  if (g == lia::btrue) return set;
  std::vector<std::int32_t> out;
  for (const std::int32_t v : elements(set)) {
    const std::int32_t env[] = {v};
    if (exprs_.eval_bool(g, env) == lia::expr_factory::truth::yes) {
      out.push_back(v);  // ⊥ excludes, like a failed guard
    }
  }
  return of_sorted(out);
}

core::code int_set_theory::term_sum(core::code a, core::code b) {
  if (a == b) return a;
  if (a > b) std::swap(a, b);  // sum is commutative: one code for both orders
  return terms_.get(
      int_term{int_shape::sum, int_action::keep, int_guard::none, 0, a, b});
}

core::code int_set_theory::term_lfp(core::code t) {
  if (t == 0) return 0;  // lfp(id) = id
  const int_term& inner = terms_[t];
  if (inner.shape == int_shape::lfp) return t;  // idempotent
  if (inner.shape == int_shape::primitive &&
      inner.action == int_action::keep) {
    // A pure guard only removes values, so its accumulation adds nothing:
    // lfp(g) = id. A sound fact about lfp — not about bare star, whose
    // g* would be g. Cheaper bill, same meaning.
    return 0;
  }
  return terms_.get(
      int_term{int_shape::lfp, int_action::keep, int_guard::none, 0, t, 0});
}

core::code int_set_theory::apply_local(core::code term, core::code value) {
  if (term == 0) return value;        // id is free
  if (value == core::none) return core::none;

  const int_term& t = terms_[term];
  if (t.shape == int_shape::sum) {
    return join(apply_local(t.a, value), apply_local(t.b, value));
  }
  if (t.shape == int_shape::lfp) {
    // Naive iteration. A theory is free to fuse a closure instead; this one
    // does not try, which is what makes it the honest oracle.
    core::code x = value;
    for (;;) {
      const core::code y = join(x, apply_local(t.a, x));
      if (y == x) return x;
      x = y;
    }
  }

  core::code kept = value;
  switch (t.gkind) {
    case int_guard::none: break;
    case int_guard::set: kept = meet(value, t.a); break;
    case int_guard::symbolic: kept = filter(value, t.a); break;
  }
  if (kept == core::none) return core::none;  // the guard refused: deadlock

  switch (t.action) {
    case int_action::keep:
      return kept;
    case int_action::assign:
      return singleton(t.arg);
    case int_action::shift: {
      // The pushforward of x -> x + delta. Order is preserved, so the run stays
      // sorted and no re-sorting is owed. The add is overflow-checked: a value
      // that leaves the representable range is a loud failure, never a silent
      // wrap — it is the theory's job to represent classes finitely, and when
      // it cannot (an unbounded net), it says so here rather than lie.
      const auto from = elements(kept);
      std::vector<std::int32_t> out;
      out.reserve(from.size());
      for (const std::int32_t v : from) {
        std::int32_t nv;
        if (__builtin_add_overflow(v, t.arg, &nv)) {
          throw overflow_error("int32 overflow shifting " + std::to_string(v) +
                               " by " + std::to_string(t.arg));
        }
        out.push_back(nv);
      }
      return of_sorted(out);
    }
    case int_action::xform: {
      // x := e(x) per element. ⊥ drops the element (abort is the algebra's
      // 0); a modulo wraps into [0, arg) (DVE byte semantics); otherwise a
      // result outside int32 is loud, like shift.
      std::vector<std::int32_t> out;
      for (const std::int32_t v : elements(kept)) {
        const std::int32_t env[] = {v};
        bool undef = false;
        std::int64_t r = exprs_.eval_int(t.b, env, undef);
        if (undef) continue;
        if (t.arg != 0) {
          r = ((r % t.arg) + t.arg) % t.arg;
        } else if (r < INT32_MIN || r > INT32_MAX) {
          throw overflow_error("int32 overflow transforming " +
                               std::to_string(v));
        }
        out.push_back(static_cast<std::int32_t>(r));
      }
      return of(out);
    }
    case int_action::havoc:
      // any value of the range, whatever passed the guard
      return interval(t.arg, static_cast<std::int32_t>(t.b));
    case int_action::choose:
      // any value of the set, whatever passed the guard
      return t.b;
  }
  return core::none;
}

std::vector<std::pair<lia::iexpr, core::code>> int_set_theory::split_equiv(
    core::code set, lia::iexpr e) {
  std::vector<std::pair<lia::iexpr, core::code>> classes;
  if (set == core::none) return classes;

  // Group the elements by the value of e. Element lists stay ascending
  // because the input elements are, so `of_sorted` needs no re-sort.
  std::map<lia::iexpr, std::vector<std::int32_t>> by_marker;
  std::int32_t env[1];
  for (const std::int32_t x : elements(set)) {
    env[0] = x;
    bool undef = false;
    const std::int64_t v = exprs_.eval_int(e, env, undef);
    lia::iexpr marker = lia::iundef;
    if (!undef) {
      if (v < INT32_MIN || v > INT32_MAX) {
        throw overflow_error("int32 overflow evaluating split_equiv on " +
                             std::to_string(x));
      }
      marker = exprs_.constant(static_cast<std::int32_t>(v));
    }
    by_marker[marker].push_back(x);
  }

  classes.reserve(by_marker.size());
  for (auto& [m, xs] : by_marker) classes.emplace_back(m, of_sorted(xs));
  return classes;
}

core::code int_set_theory::join(core::code a, core::code b) {
  if (a == b) return a;
  const auto x = elements(a);
  const auto y = elements(b);
  std::vector<std::int32_t> out;
  out.reserve(x.size() + y.size());
  std::ranges::set_union(x, y, std::back_inserter(out));
  return of_sorted(out);
}

core::code int_set_theory::meet(core::code a, core::code b) {
  if (a == b) return a;
  const auto x = elements(a);
  const auto y = elements(b);
  std::vector<std::int32_t> out;
  out.reserve(std::min(x.size(), y.size()));
  std::ranges::set_intersection(x, y, std::back_inserter(out));
  return of_sorted(out);
}

core::code int_set_theory::minus(core::code a, core::code b) {
  if (a == b) return core::none;
  const auto x = elements(a);
  const auto y = elements(b);
  std::vector<std::int32_t> out;
  out.reserve(x.size());
  std::ranges::set_difference(x, y, std::back_inserter(out));
  return of_sorted(out);
}

double int_set_theory::cardinal(core::code c) const {
  return static_cast<double>(elements(c).size());
}

void int_set_theory::print(std::ostream& os, core::code c) const {
  os << '{';
  bool first = true;
  for (const std::int32_t v : elements(c)) {
    if (!first) os << ',';
    first = false;
    os << v;
  }
  os << '}';
}

}  // namespace hsc::leaves
