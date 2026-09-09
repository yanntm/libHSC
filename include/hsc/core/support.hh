/// \file support.hh
/// \brief The support algebra a sort exports.
///
/// The contract is joins, meets, and **relative** differences. There is no
/// top and no complement, and no construction in libHSC forms one. Decidable
/// equality and emptiness are also owed, but interning discharges both for
/// free, so what an implementor actually writes is three operations.
#pragma once

#include <iosfwd>

#include "hsc/core/code.hh"
#include "hsc/util/errors.hh"

namespace hsc::core {

/// \brief What a sort must provide for its codes to be primes at a cut.
///
/// Type-erased rather than a template parameter: a head position's primes
/// live in a leaf theory's algebra or — when the head is itself composite —
/// in the diagram algebra, and which one it is is what the *shape* says.
/// Shapes are data, so the dispatch is a runtime one. See `algorithm.md` §3.
///
/// Diagrams implement this interface like any other theory. That is
/// internalisation made structural: there is no leaf case and node case.
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

  /// \brief Apply a local term to a code.
  ///
  /// The term is handed over **whole** — guards, assigns, composition, sum,
  /// fused as the implementor sees fit. Splitting a code, acting per piece
  /// and re-joining is what the theory contract forbids doing from outside.
  ///
  /// Term code `0` is `id`: free, never represented, and callers are
  /// expected to short-circuit it rather than dispatch on it.
  ///
  /// Which codes are legal terms is the implementor's business: a leaf
  /// theory interprets its own term language, and the diagram engine
  /// interprets operation terms — that is internalisation, and it is why this
  /// method is on this interface rather than on a separate one.
  virtual code apply_local(code term, code value) = 0;

  /// \name Building local terms
  ///
  /// A sort must be able to combine and close terms of its own language, so
  /// that the saturation rewrite can push a closure down to it without
  /// knowing what the language is.
  ///@{
  /// The term acting as \p a or \p b.
  virtual code term_sum(code a, code b) = 0;
  /// \brief The term acting as \p before then \p after, fused into one local
  /// term (`algorithm.md` §11). An **optional** capability: the default
  /// refuses with `unsupported_error`, and the caller keeps an unfused
  /// composition instead.
  virtual code term_compose(code after, code before);
  /// \brief The least fixpoint `lfp(t) = (id + t)*`, as a term.
  ///
  /// The derived form of the theory contract's pure star, offered as the
  /// primitive so that recognizing an accumulation never requires matching
  /// operands. Bare (non-reflexive) star is deliberately not on this
  /// interface: nothing in the calculus asks for it yet.
  virtual code term_lfp(code t) = 0;
  ///@}

  /// \brief The existential image: a nonempty subset of `term(value)`, or
  /// `none` exactly when that image is empty (`algorithm.md` §10). The
  /// default computes the image in full, which is always correct; a theory
  /// may stop at the first element that passes.
  virtual code has_image_local(code term, code value) {
    return apply_local(term, value);
  }
  /// \brief The converse of a local term, restricted to \p domain.
  ///
  /// Every term denotes an additive map, hence a relation; this is the
  /// term of the converse relation whose results are kept inside
  /// \p domain (a code of this algebra, the coordinate's finite potential).
  /// An **optional** capability, not a preimage owed by the contract: the
  /// default refuses with `unsupported_error`, and the calculus reports
  /// rather than approximates. See `algorithm.md` §9.
  virtual code invert_local(code term, code domain);
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

inline code support_algebra::term_compose(code, code) {
  throw unsupported_error("this theory does not compose its local terms");
}

inline code support_algebra::invert_local(code, code) {
  throw unsupported_error("this theory does not invert its local terms");
}

}  // namespace hsc::core
