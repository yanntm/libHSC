/// \file rewrite.hh
/// \brief The rewrite chain: semantically neutral `datum → datum` passes
/// between the parametric pass and the translator, each returning its
/// forms **plus a trace** — which pass, whether it applied, what it did.
///
/// Identity is reported, never silent. Traces are free text for now; the
/// format will tighten later — provenance is the contract, not yet its
/// shape. `algorithm.md` §1c.
#pragma once

#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "hsc/surface/sexpr.hh"

namespace hsc::surface {

/// What one transform returns: the (possibly identical) forms, whether
/// anything was done, and the free-text account of it.
struct rewrite_result {
  std::vector<datum> forms;
  bool applied = false;  ///< false: identity — `forms` is the input
  std::string trace;
};

using transform = std::function<rewrite_result(std::vector<datum>)>;

/// A named pass of the chain. Assumptions on the input's form (binder
/// free, shape declared, …) are part of the pass's documentation.
struct pass {
  std::string name;
  transform apply;
};

/// One entry of a chain's provenance record: who, whether, what.
struct trace_entry {
  std::string pass_name;
  bool applied = false;
  std::string trace;
};

/// Run \p passes in order over \p forms; collect one record per pass.
[[nodiscard]] std::pair<std::vector<datum>, std::vector<trace_entry>>
rewrite(std::vector<datum> forms, std::span<const pass> passes);

/// \brief The constant-elision pass. Assumes binder-free forms (post
/// expand). A scalar leaf whose inferred domain is one value is a
/// constant: reads fold to the value, writes of it drop, the leaf leaves
/// the declarations, the shape and the init. Arrays are not elided. A
/// `(word …)` pair binding an elided leaf drops with a trace note.
[[nodiscard]] rewrite_result elide_constants(std::vector<datum> forms);

/// The default chain, in order. Grows as passes arrive.
[[nodiscard]] std::vector<pass> default_chain();

}  // namespace hsc::surface
