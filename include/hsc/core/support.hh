/// \file support.hh
/// \brief The support algebra a sort exports (§2.1), at tier G.
///
/// Tier G is the working tier: joins, meets, and **relative** differences.
/// There is no top and no complement, and no construction in libHSC forms
/// one. Tiers E and J also owe decidable equality and emptiness; interning
/// discharges both for free, so what an implementor actually writes is three
/// operations.
#pragma once

#include <iosfwd>

#include "hsc/core/code.hh"

namespace hsc::core {

/// \brief What a sort must provide for its codes to be primes at a cut.
///
/// Type-erased rather than a template parameter: a head position's primes
/// live in a leaf theory's algebra or — when the head is itself composite —
/// in the diagram algebra, and which one it is is what the *shape* says.
/// Shapes are data, so the dispatch is a runtime one. See `algorithm.md` §3.
///
/// Diagrams implement this interface like any other theory. That is
/// Corollary 3.6 (internalisation carries operations) made structural: there
/// is no leaf case and node case.
class support_algebra {
 public:
  virtual ~support_algebra() = default;

  support_algebra() = default;
  support_algebra(const support_algebra&) = delete;
  support_algebra& operator=(const support_algebra&) = delete;

  /// \name Tier G, less what interning gives for free
  ///
  /// Every implementation may assume both arguments are nonzero: `none` is
  /// absence and is handled by the caller, never dispatched on.
  ///@{
  virtual code join(code a, code b) = 0;   ///< a ∪ b
  virtual code meet(code a, code b) = 0;   ///< a ∩ b
  virtual code minus(code a, code b) = 0;  ///< a ∖ b, relative
  ///@}

  /// \brief How many elements \p c denotes.
  ///
  /// A double because state spaces are exponential and this number is for
  /// reading, not for deciding.
  [[nodiscard]] virtual double cardinal(code c) const = 0;

  /// Render \p c for a human.
  virtual void print(std::ostream& os, code c) const = 0;

  /// \name The free part of the contract
  ///
  /// Emptiness and equality are decided on codes alone. Present as static
  /// functions so that no call site is tempted to ask a theory.
  ///@{
  [[nodiscard]] static constexpr bool empty(code c) noexcept {
    return c == none;
  }
  [[nodiscard]] static constexpr bool equal(code a, code b) noexcept {
    return a == b;
  }
  ///@}
};

}  // namespace hsc::core
