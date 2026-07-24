#include "hsc/leaves/int_set.hh"

#include <algorithm>
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

core::code int_set_theory::term_closure(core::code t) {
  if (t == 0) return 0;  // id* = id
  const int_term& inner = terms_[t];
  if (inner.shape == int_shape::closure) return t;  // idempotent
  if (inner.shape == int_shape::primitive &&
      inner.action == int_action::keep) {
    // A pure guard only removes values, so under union it adds nothing:
    // (g + id)* = id. Discipline 5 -- this is a cheaper bill, same meaning.
    return 0;
  }
  return terms_.get(
      int_term{int_shape::closure, int_action::keep, int_guard::none, 0, t, 0});
}

core::code int_set_theory::apply_local(core::code term, core::code value) {
  if (term == 0) return value;        // id is free
  if (value == core::none) return core::none;

  const int_term& t = terms_[term];
  if (t.shape == int_shape::sum) {
    return join(apply_local(t.a, value), apply_local(t.b, value));
  }
  if (t.shape == int_shape::closure) {
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
  }
  return core::none;
}

std::vector<std::pair<std::int32_t, core::code>> int_set_theory::split_equiv(
    core::code set, std::int32_t coeff, std::int32_t offset) {
  std::vector<std::pair<std::int32_t, core::code>> classes;
  if (set == core::none) return classes;

  // Group the elements by the value coeff*x + offset. `std::map` keeps the
  // classes ascending by value; each element list stays ascending because the
  // input elements are, so `of_sorted` needs no re-sort.
  std::map<std::int32_t, std::vector<std::int32_t>> by_value;
  for (const std::int32_t x : elements(set)) {
    std::int32_t v;
    if (__builtin_mul_overflow(coeff, x, &v) ||
        __builtin_add_overflow(v, offset, &v)) {
      throw overflow_error("int32 overflow evaluating split_equiv on " +
                           std::to_string(x));
    }
    by_value[v].push_back(x);
  }

  classes.reserve(by_value.size());
  for (auto& [value, xs] : by_value) classes.emplace_back(value, of_sorted(xs));
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
