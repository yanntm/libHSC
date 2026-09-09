/// \file to_surface.hh
/// \brief M2T: a parsed P/T net plus its NUPN unit tree → `.hsc` surface text.
///
/// One leaf per place, the shape read off the unit tree verbatim (no tricks),
/// one event per transition: the guard is the input arcs (`>= w`), the action
/// the net per-place effect (`+=`/`-=`). Reachability is composed from the
/// generic algebra, not a bespoke command. Every transition is separable — a
/// P/T transition's guard and update are per-place (Cor 6.3) — so nothing here
/// reaches for `split_equiv`.
#pragma once

#include <iosfwd>

#include "hsc/petri/core/SparsePetriNet.h"
#include "hsc/petri/nupn.hh"

namespace hsc::petri {

/// Which trailing query the emitted model runs. `none` emits the plain
/// reach/count/nodes/bill; `state_space` emits the MCC command, `one_safe`
/// the general forms (`reach` + `max-value`) its runner turns into the MCC
/// verdict.
/// What follows the model: `none` the default reach with its meters (the
/// `nupn2hsc` output), `model_only` nothing (a driver adds its own forms),
/// or an MCC examination's query.
enum class examination { none, model_only, state_space, one_safe };

struct emit_options {
  int bound = 2;  ///< leaf domain is [0, bound); a safe NUPN needs only 2
  /// Leave out the events that cannot change the marking (every per-place
  /// effect zero: a read-arc-only or self-looping transition). They add
  /// nothing to the reachable set, so a consumer that only needs the fixpoint
  /// pays less; one that counts arcs or dead markings reads their guards from
  /// the net instead, where they remain. A consumer asking temporal
  /// questions must keep them: they are the self-loops of the graph.
  bool skip_no_effect = false;
  examination exam = examination::none;
  /// A shape given verbatim — the `(spine …)` / `(balanced …)` expression over
  /// the place names, as `(print-spec)` echoes it — used instead of the unit
  /// tree when non-empty. The reified order: a file any tool can write.
  std::string shape_form;
};

/// \brief Write the `.hsc` model for \p net (with unit tree \p units) to \p out.
/// If \p units is absent, the shape falls back to a flat spine over all places.
void to_surface(std::ostream& out, const SparsePetriNet<int>& net,
                const unit_tree& units, const emit_options& opts = {});

}  // namespace hsc::petri
