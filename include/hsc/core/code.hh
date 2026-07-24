/// \file code.hh
/// \brief Codes: the citizens of the calculus.
///
/// Every object a computation touches is a code — a finite, canonical,
/// hashable name. A code is an id into an intern table, and which table is
/// settled by the sort, not carried by the code.
#pragma once

#include <cstdint>

namespace hsc::core {

/// A canonical name. Which algebra it names is decided by the shape.
using code = std::uint32_t;

/// \brief Absence: the pointed discipline's adjoined `0`.
///
/// A legal *answer*, never a legal *letter*. Nothing canonical stores,
/// traverses or points to it — which is what makes emptiness a comparison
/// against an integer rather than a question for a theory.
inline constexpr code none = 0;

/// A shape code. Distinguished in signatures only for readability.
using shape_code = std::uint32_t;

/// Index of an imported leaf theory in the manager's registry.
using theory_index = std::uint32_t;

}  // namespace hsc::core
