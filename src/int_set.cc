#include "hsc/leaves/int_set.hh"

#include <algorithm>
#include <ostream>
#include <string>

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
  return terms_.get(int_term{int_shape::primitive, int_action::keep, 0, set, 0});
}

core::code int_set_theory::assign(core::code set, std::int32_t value) {
  return terms_.get(
      int_term{int_shape::primitive, int_action::assign, value, set, 0});
}

core::code int_set_theory::shift(core::code set, std::int32_t delta) {
  if (delta == 0) return set == core::none ? core::code{0} : keep(set);
  return terms_.get(
      int_term{int_shape::primitive, int_action::shift, delta, set, 0});
}

core::code int_set_theory::term_sum(core::code a, core::code b) {
  if (a == b) return a;
  if (a > b) std::swap(a, b);  // sum is commutative: one code for both orders
  return terms_.get(int_term{int_shape::sum, int_action::keep, 0, a, b});
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
  return terms_.get(int_term{int_shape::closure, int_action::keep, 0, t, 0});
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
    // does not try, which is what makes it the honest oracle (§2.5).
    core::code x = value;
    for (;;) {
      const core::code y = join(x, apply_local(t.a, x));
      if (y == x) return x;
      x = y;
    }
  }

  const core::code kept = t.a == core::none ? value : meet(value, t.a);
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
  }
  return core::none;
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
