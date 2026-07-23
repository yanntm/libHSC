/// \file errors.hh
/// \brief Loud failures the calculus raises rather than papering over.
#pragma once

#include <stdexcept>

namespace hsc {

/// \brief A value left the finite domain a bounded leaf can represent.
///
/// Raised, never swallowed: silently truncating an out-of-domain value would
/// lose a place's real behaviour and answer a different question. It is up to a
/// leaf algebra to represent its classes finitely (a covering graph, a wider
/// domain); when it cannot, it says so here. libDDD/ITS thread this discipline
/// throughout, and client instances *will* overflow — failing loudly is fine,
/// erroring silently is not.
struct overflow_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

}  // namespace hsc
