/// \file domains.hh
/// \brief Domain inference, the decoration step: which units (scalars,
/// whole arrays) can be statically shown to hold a small effective
/// domain, from initial values and assignments alone.
///
/// The GAL DomainAnalyzer re-expressed over the explicit model, extended
/// with mod-k, havoc, boolean right-hand sides, and an exact value set
/// kept until a cap (so holes — sentinel patterns like {0,1,2,255} — stay
/// visible). Algorithm in `algorithm.md` §5. No search is run.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "hsc/xpl/interpret/model.hh"
#include "hsc/xpl/state.hh"

namespace hsc::xpl {

struct domain_report {
  enum class kind : std::uint8_t {
    set,       ///< the exact value set, ≤ the cap
    interval,  ///< hull only: the set outgrew the cap, or mod/havoc range
    top        ///< unconstrained: some assignment defied analysis
  };
  std::vector<std::uint32_t> positions;  ///< the unit, ascending
  kind k = kind::top;
  bool assigned = false;  ///< false: frozen — its seeds are its whole life
  bool via_mod = false;   ///< an interval came from `% k` (operand assumed
                          ///< nonnegative, as an index is)
  bool widened = false;   ///< the fixpoint's round cap fired on this unit:
                          ///< widened to a guard/mod threshold, or to top
  bool walk_budget_hit = false;  ///< the gather budget fired (same value on
                                 ///< every report): some assignments were
                                 ///< processed constraint-free
  std::vector<value> values;  ///< kind set: ascending
  value lo = 0, hi = 0;       ///< kind interval: inclusive hull
};

/// Infer per-unit domains of \p m. Units are scalar positions merged with
/// \p groups (the declared arrays) and with every multi-cell target or
/// array node — one domain per array, never refined per cell. \p seeds
/// contribute their values (the initial states).
[[nodiscard]] std::vector<domain_report> infer_domains(
    const model& m, std::span<const word> seeds,
    std::span<const std::vector<std::uint32_t>> groups);

}  // namespace hsc::xpl
