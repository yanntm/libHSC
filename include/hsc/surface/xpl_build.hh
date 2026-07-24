/// \file xpl_build.hh
/// \brief Surface event forms → an `xpl::model`, the explicit transition
/// relation.
///
/// The bridge between the surface and the explicit engine: a second, much
/// smaller compile of the same `(when …)`/`(do …)` clauses, into guards
/// and assignments over frontier positions instead of operation terms.
/// `xpl` itself never sees a `datum` or a diagram; this header is surface
/// code.
#pragma once

#include <span>
#include <string>
#include <vector>

#include "hsc/surface/expr.hh"
#include "hsc/surface/sexpr.hh"
#include "hsc/xpl/interpret/model.hh"

namespace hsc::surface {

/// One event as the translator declared it: name, source line, and the
/// clause forms — retained verbatim, meaning assigned here.
struct xpl_source {
  std::string name;
  int line = 0;
  std::vector<datum> clauses;
};

/// \brief Compile \p sources into an explicit model over \p arity
/// positions, resolving names through \p scope and reading expressions
/// with \p reader. The returned model is finalized (supports computed).
/// Throws `translate_error` on a malformed clause.
[[nodiscard]] xpl::model build_xpl_model(std::size_t arity,
                                         lia::expr_factory& ex,
                                         const expr_reader& reader,
                                         const name_scope& scope,
                                         std::span<const xpl_source> sources);

}  // namespace hsc::surface
