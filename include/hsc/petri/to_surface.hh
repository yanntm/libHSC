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

#include "hsc/petri/SparsePetriNet.h"
#include "hsc/petri/nupn.hh"

namespace hsc::petri {

/// Which trailing query the emitted model runs. `none` emits the plain
/// reach/count/nodes/bill; the others emit a single MCC examination command.
enum class examination { none, state_space, one_safe, deadlock };

struct emit_options {
  int bound = 2;  ///< leaf domain is [0, bound); a safe NUPN needs only 2
  examination exam = examination::none;
};

/// \brief Write the `.hsc` model for \p net (with unit tree \p units) to \p out.
/// If \p units is absent, the shape falls back to a flat spine over all places.
void to_surface(std::ostream& out, const SparsePetriNet<int>& net,
                const unit_tree& units, const emit_options& opts = {});

}  // namespace hsc::petri
