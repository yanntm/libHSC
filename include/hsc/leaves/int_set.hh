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
#include <vector>

#include "hsc/core/support.hh"
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

/// \brief A local term of this theory (Def 2.3): a guard, then an action.
///
/// Small and closed on purpose. It is enough for a Petri transition
/// (`m >= w` then `m -= w`) and for a Hanoi move (`pos == a` then
/// `pos := b`), which is the whole of the non-crossing fragment. Only
/// pushforwards appear — Def 2.2 exports no preimage and none is wanted.
struct int_term {
  core::code guard = core::none;  ///< `none` means no guard
  int_action action = int_action::keep;
  std::int32_t arg = 0;

  friend bool operator==(const int_term&, const int_term&) = default;
  [[nodiscard]] std::size_t hash() const {
    return util::hash_all(guard, static_cast<std::uint8_t>(action), arg);
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
  ///@}

  core::code apply_local(core::code term, core::code value) override;

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
  std::vector<std::int32_t> scratch_;  ///< reused by of() and interval()
};

}  // namespace hsc::leaves
