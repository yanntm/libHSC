/// Shape-directed conjunction filtering; see algorithm.md.
#pragma once
#include <cstddef>
#include <span>
#include <utility>
#include <vector>
#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"

namespace hsc::core { class manager; }
namespace hsc::leaves { class int_set_theory; }
namespace hsc::linear {

struct constraint {
  std::vector<std::pair<std::size_t, long long>> terms;
  long long target = 0;
  bool at_most = false;
};

/// Exact subset of input satisfying all constraints. Positions are rooted at
/// top. Uses the manager's shared deadline, throws interrupted on expiry;
/// no partial result is returned. Checked arithmetic throws overflow_error.
/// The evaluator and its caches live only for this call.
[[nodiscard]] core::code conjunction(core::manager& mgr, leaves::int_set_theory& leaves,
                                      core::shape_code top, core::code input,
                                      std::span<const constraint> constraints);
}  // namespace hsc::linear
