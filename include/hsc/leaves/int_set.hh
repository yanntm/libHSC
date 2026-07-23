/// \file int_set.hh
/// \brief Finite sets of integers: the enumeration-honest reference theory.
///
/// The simplest thing that satisfies the tier-G contract, and deliberately so.
/// A theory earns its keep by returning few structured codes where enumeration
/// would return many singletons (§2.5); this one does not try, which is what
/// makes it the permanent differential oracle for the ones that do.
#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "hsc/core/support.hh"
#include "hsc/lia/expr.hh"
#include "hsc/mem/intern.hh"
#include "hsc/util/hash.hh"

namespace hsc::leaves {

/// \brief A sorted, duplicate-free run of integers, header plus trailing array.
struct int_set {
  std::uint32_t count;

  [[nodiscard]] const std::int32_t* data() const {
    return reinterpret_cast<const std::int32_t*>(this + 1);
  }
  [[nodiscard]] std::span<const std::int32_t> elements() const {
    return {data(), count};
  }

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_value(count);
    util::hash_range(seed, data(), data() + count);
    return seed;
  }
  friend bool operator==(const int_set& a, const int_set& b) {
    return a.count == b.count &&
           std::equal(a.data(), a.data() + a.count, b.data());
  }
};

/// \brief What a local term does to a set, after its guard.
enum class int_action : std::uint8_t {
  keep,    ///< nothing: the term is a pure guard
  assign,  ///< x := arg
  shift,   ///< x := x + arg
};

/// How a term is built.
enum class int_shape : std::uint8_t {
  primitive,  ///< a guard, then an action
  sum,        ///< a or b
  closure,    ///< `(a + id)*`
};

/// What a primitive term's guard is.
enum class int_guard : std::uint8_t {
  none,      ///< no guard: every value passes
  set,       ///< extensional: `a` is an int_set code, guard is membership
  symbolic,  ///< `a` is a `lia::bexpr` over the coordinate (position 0)
};

/// \brief A local term of this theory (Def 2.3).
///
/// A guard then an action, plus sum and star closure — which is the term
/// language Def 2.3 says a theory is handed *whole*. Enough for a Petri
/// transition (`m >= w` then `m -= w`) and a Hanoi move (`pos == a` then
/// `pos := b`). A guard is symbolic (an interned expression, evaluated on
/// the values present — no domain is materialized) or an extensional set
/// where the model genuinely enumerates. Only pushforwards appear: Def 2.2
/// exports no preimage and none is wanted.
struct int_term {
  int_shape shape = int_shape::primitive;
  int_action action = int_action::keep;
  int_guard gkind = int_guard::none;
  std::int32_t arg = 0;
  core::code a = core::none;  ///< primitive: the guard; sum/closure: operand
  core::code b = core::none;  ///< sum: the second operand

  friend bool operator==(const int_term&, const int_term&) = default;
  [[nodiscard]] std::size_t hash() const {
    return util::hash_all(static_cast<std::uint8_t>(shape),
                          static_cast<std::uint8_t>(action),
                          static_cast<std::uint8_t>(gkind), arg, a, b);
  }
};

/// \brief The theory. Owns its codes; hands out nothing but ids.
class int_set_theory final : public core::support_algebra {
 public:
  /// \brief The code for \p values, which need be neither sorted nor unique.
  /// The empty set is `none`: absence is not a citizen.
  core::code of(std::span<const std::int32_t> values);

  /// The code for `{v}`.
  core::code singleton(std::int32_t v);

  /// The code for the half-open interval `[lo, hi)`.
  core::code interval(std::int32_t lo, std::int32_t hi);

  /// The elements of \p c, ascending. Empty for `none`.
  [[nodiscard]] std::span<const std::int32_t> elements(core::code c) const;

  /// \brief `split_equiv` (§7): partition \p set by the value of the linear
  /// expression `coeff*x + offset` over this leaf's coordinate `x`. Returns
  /// `{value, sub-code}` pairs, ascending by value, one per distinct value the
  /// expression takes — the sub-code holds the elements yielding it.
  ///
  /// Exact: one class per value, no cost knob (the added expressiveness of §7).
  /// The value is a code of the interchange (LIA) domain — here a plain integer.
  /// `coeff*x + offset` is overflow-checked, loud on leaving int32 range.
  [[nodiscard]] std::vector<std::pair<std::int32_t, core::code>> split_equiv(
      core::code set, std::int32_t coeff = 1, std::int32_t offset = 0);

  /// \name Local terms
  ///
  /// Term code 0 is `id`, as everywhere: free and never interned.
  ///@{
  /// Keep only values in \p set.
  core::code keep(core::code set);
  /// `x := value`, guarded by \p set (`none` for no guard).
  core::code assign(core::code set, std::int32_t value);
  /// `x := x + delta`, guarded by \p set (`none` for no guard).
  core::code shift(core::code set, std::int32_t delta);
  /// The symbolic twins: guarded by \p g, a `bexpr` over the coordinate
  /// (`lia` position 0), evaluated on the values present. `btrue` folds to
  /// the unguarded form; a `bfalse` guard is the caller's dead term to drop.
  core::code keep_if(lia::bexpr g);
  core::code assign_if(lia::bexpr g, std::int32_t value);
  core::code shift_if(lia::bexpr g, std::int32_t delta);
  ///@}

  /// The elements of \p set on which \p g evaluates to true (⊥ excludes).
  core::code filter(core::code set, lia::bexpr g);

  /// The expression factory guards are written in — shared with every layer
  /// that builds or reads criteria over this theory's coordinates.
  [[nodiscard]] lia::expr_factory& exprs() { return exprs_; }

  core::code apply_local(core::code term, core::code value) override;
  core::code term_sum(core::code a, core::code b) override;
  core::code term_closure(core::code t) override;

  core::code join(core::code a, core::code b) override;
  core::code meet(core::code a, core::code b) override;
  core::code minus(core::code a, core::code b) override;
  [[nodiscard]] double cardinal(core::code c) const override;
  void print(std::ostream& os, core::code c) const override;

  [[nodiscard]] const mem::intern_statistics& stats() const {
    return table_.stats();
  }

 private:
  /// Intern an already sorted, duplicate-free run. Empty gives `none`.
  core::code of_sorted(std::span<const std::int32_t> values);

  mem::intern<int_set> table_;
  mem::intern<int_term> terms_;
  lia::expr_factory exprs_;
  std::vector<std::int32_t> scratch_;  ///< reused by of() and interval()
};

}  // namespace hsc::leaves
