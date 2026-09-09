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

/// \brief The one-hot pass (`algorithm.md` §1c): an eligible enumerated
/// leaf trades its integer for K bits, one per value, exactly one set.
/// \p directive is the invoking form — `(hotbit [MIN [MAX]])`, the
/// eligibility window on domain size, defaults 3 and 16.
[[nodiscard]] rewrite_result hotbit(std::vector<datum> forms,
                                    const datum& directive);

/// \brief `(reorder-force)`: run FORCE on the spec's own event supports
/// (one clique per event); the shape becomes the resulting flat spine.
[[nodiscard]] rewrite_result reorder_force(std::vector<datum> forms,
                                           const datum& directive);
/// \brief `(reorder-reverse)`: the mirror image of the shape at every
/// level — same hierarchy, every node's children in reverse. A variation
/// point on top of FORCE: which end of an order sits at the top matters.
[[nodiscard]] rewrite_result reorder_reverse(std::vector<datum> forms,
                                             const datum& directive);

/// \brief `(flatten)`: the shape becomes the flat spine of the current
/// frontier order — hierarchy deliberately erased.
[[nodiscard]] rewrite_result flatten(std::vector<datum> forms,
                                     const datum& directive);

/// \brief `(simplify-arrays)`: an array with no dynamic access
/// dissolves — static accesses fold to their cell names, the grouping
/// goes, supports stop over-approximating to the whole array.
[[nodiscard]] rewrite_result simplify_arrays(std::vector<datum> forms,
                                             const datum& directive);

/// \brief `(declare-domains)`: every bare scalar `(leaf NAME)` whose
/// inferred domain is finite (a value set, or an interval hull) gets the
/// inference written into its declaration — `(leaf NAME LO HI)` — and
/// every declared leaf whose inferred hull is strictly tighter is
/// re-declared to the hull. Never widened; a leaf whose domain defies
/// analysis stays bare, with a trace note. Neutral for any run (the
/// inference over-approximates every assignable value, so no firing
/// write becomes an out-of-bounds error); what it buys is the finite
/// instance at its true size: engines whose costs scale with the
/// declared domain — leaf state counts, letter induction — stop paying
/// for values a leaf never holds.
[[nodiscard]] rewrite_result declare_domains(std::vector<datum> forms);

/// \brief `(decompose-louvain)`: a hierarchical shape from the spec's
/// own dependency structure — Louvain clustering over the control→write
/// co-occurrence graph of the events; communities become nested
/// `(balanced …)` blocks.
[[nodiscard]] rewrite_result decompose_louvain(std::vector<datum> forms,
                                               const datum& directive);

/// A pass as the registry serves it: the directive form is handed
/// through, so a pass can take arguments.
struct pass_def {
  std::string name;
  std::function<rewrite_result(std::vector<datum>, const datum&)> apply;
};

/// Every pass a directive can invoke, by name.
[[nodiscard]] std::vector<pass_def> pass_registry();

}  // namespace hsc::surface
