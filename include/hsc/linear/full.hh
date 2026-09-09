/// \file full.hh
/// \brief The full product of the leaf domains as a diagram (`algorithm.md` §1).
///
/// Along the shape: a pair sort's node has one arc per value of its head
/// leaf (or one arc whose prime is the head subshape's own product), the
/// tail's product as the sub. The caller says what a leaf position's domain
/// is, as a code of the leaf's theory — the only thing this client needs
/// from the theory.
#pragma once
#include <cstddef>
#include <functional>

#include "hsc/core/code.hh"
#include "hsc/core/shape.hh"

namespace hsc::core {
class manager;
}

namespace hsc::linear {

/// \brief The diagram of every word whose leaf at position `p` lies in
/// `domain(p)`, under the shape rooted at \p top. `none` when a domain is
/// empty.
[[nodiscard]] core::code full_box(core::manager& mgr, core::shape_code top,
                                  const std::function<core::code(std::size_t)>& domain);

}  // namespace hsc::linear
