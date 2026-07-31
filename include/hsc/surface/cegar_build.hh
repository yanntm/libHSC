/// \file cegar_build.hh
/// \brief Parsed `.hsc` spec + property atoms → a `cegar::model`.
///
/// The bridge accepts exactly the separable fragment (spec §5 of
/// `research_notes/cegar_spec.md`): bounded leaves, one seed, plain
/// guarded-command events whose guard atoms and actions are each local
/// to one leaf, no arrays, no havoc. Per-leaf letters are the distinct
/// local-action graphs, induced by enumeration over the leaf's domain
/// and numbered canonically (lex-sorted graphs). The monitor is the
/// exact sub-product of the leaves the atoms name. Everything outside
/// the fragment is refused with a `translate_error` naming the event
/// and the construct — refusals are the paper's deferred relaxations.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "hsc/cegar/model.hh"
#include "hsc/surface/expr.hh"
#include "hsc/surface/spec.hh"

namespace hsc::surface {

struct cegar_bridge {
  cegar::model model;
  /// The full-arity seed word (model values, frontier order): the
  /// starting point for binding validated witnesses back to states.
  std::vector<std::int32_t> seed;
};

/// Build the loop's model from \p s under property atoms \p atoms.
/// Throws `translate_error` on any construct outside the fragment.
[[nodiscard]] cegar_bridge build_cegar_model(const spec& s,
                                             lia::expr_factory& ex,
                                             const expr_reader& reader,
                                             std::span<const datum> atoms);

}  // namespace hsc::surface
