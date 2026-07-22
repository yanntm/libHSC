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

  /// \brief Apply a local term to a code (Def 2.3).
  ///
  /// The term is handed over **whole** — guards, assigns, composition, sum,
  /// fused as the implementor sees fit. Splitting a code, acting per piece
  /// and re-joining is what Def 2.3 forbids doing to a theory from outside.
  ///
  /// Term code `0` is `id`: free, never represented, and callers are
  /// expected to short-circuit it rather than dispatch on it.
  ///
  /// Which codes are legal terms is the implementor's business: a leaf
  /// theory interprets its own term language, and the diagram engine
  /// interprets operation terms — that is Corollary 3.6, and it is why this
  /// method is on this interface rather than on a separate one.
  virtual code apply_local(code term, code value) = 0;

  /// \name Building local terms
  ///
  /// A sort must be able to combine and close terms of its own language, so
  /// that the saturation rewrite (§6) can push a closure down to it without
  /// knowing what the language is.
  ///@{
  /// The term acting as \p a or \p b.
  virtual code term_sum(code a, code b) = 0;
  /// The reflexive closure `(t + id)*`, as a term.
  virtual code term_closure(code t) = 0;
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
