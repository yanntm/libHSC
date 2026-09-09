/// \file equality.hh
/// \brief The diagram of `Σ c_p · x_p = K` over the leaf domains, built
/// directly (`algorithm.md` §1): a knapsack along the shape, one node per
/// (sort, position, remaining sum), the residuals above K pruned — which is
/// why the coefficients must be nonnegative. Linear in places × K, where the
/// case-bracket curry of the same atom over a full box enumerates residual
/// classes and never returns on a few dozen constraints.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"

namespace hsc::core {
class manager;
}

namespace hsc::linear {

/// What the constructor needs from the leaves: the values of a position's
/// domain, ascending, and the leaf code of a subset of them.
struct leaf_access {
  std::function<std::span<const std::int32_t>(std::size_t)> values;
  std::function<core::code(std::size_t, std::span<const std::int32_t>)> subset;
};

/// \brief The words of the shape rooted at \p top whose leaves lie in their
/// domains and satisfy `Σ coeff[p] · x_p = k` — or `≤ k` with \p at_most, the
/// residual sums below k kept instead of pruned; `coeff` per frontier
/// position, every coefficient ≥ 0. `none` when no word does.
[[nodiscard]] core::code equality(core::manager& mgr, core::shape_code top,
                                  std::span<const long long> coeff, long long k,
                                  const leaf_access& leaves, bool at_most = false);

}  // namespace hsc::linear
