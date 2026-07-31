/// \file to_surface.hh
/// \brief M2M: ground FSP LTSs → the separable fragment of the surface
/// (`algorithm.md` §3), and the model/driver `.hsc` serialization (M2T).
///
/// One leaf per non-property process (file order), the property totalized
/// into the monitor leaf `mon` (last in the spine, error = top value), one
/// event per (ground label, target-piece tuple). Every emitted event is
/// separable by construction; whether the composition is *faithful* is the
/// triangle's business (`algorithm.md` §5).
#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

#include "hsc/fsp/ground.hh"
#include "hsc/surface/sexpr.hh"

namespace hsc::fsp {

/// \brief The transformed subject: surface forms plus what the driver and
/// the header need to know.
struct translation {
  std::vector<surface::datum> forms;   ///< leaf / shape / init / events
  std::vector<std::string> header;     ///< comment lines (provenance, maps)
  int monitor_error = 0;               ///< the monitor's error value E
  std::size_t events = 0;              ///< events emitted
  std::size_t labels = 0;              ///< ground labels composed
  std::size_t dead_labels = 0;         ///< labels some participant never fires
  std::size_t stutters = 0;            ///< all-identity tuples dropped
};

/// \brief A composition that cannot be mapped (no / several properties, a
/// nondeterministic property, ERROR outside the property, …).
class transform_error : public std::runtime_error {
 public:
  explicit transform_error(const std::string& what)
      : std::runtime_error(what) {}
};

/// \brief Compose the ground processes. Exactly one must be the property.
/// State-name maps longer than \p map_limit states are suppressed from the
/// header. Throws `transform_error`.
[[nodiscard]] translation to_surface(const std::vector<ground_lts>& procs,
                                     std::size_t map_limit = 200);

/// \brief M2T: the model-only `.hsc` (header comments, then the forms).
void print_model(std::ostream& os, const translation& t);

/// \brief The driver `.hsc`: `(input MODEL)`, the cegar query on `mon`,
/// the explicit cross-check. Expectations are pinned by the campaign, not
/// guessed here.
void print_driver(std::ostream& os, const translation& t,
                  const std::string& model_file);

}  // namespace hsc::fsp
