/// \file nupn.hh
/// \brief The NUPN unit tree carried in a PNML `<toolspecific tool="nupn">`.
///
/// The vendored `PTNetHandler` treats every `toolspecific` block as opaque, so
/// the unit hierarchy is read here, in a second lightweight SAX pass. The logic
/// mirrors `fr.lip6.move.gal.nupn.NupnHandler` (Java, same author).
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace hsc::petri {

/// One NUPN unit: the places it holds directly, and its child unit ids.
struct unit {
  std::string id;
  std::vector<std::string> places;
  std::vector<std::string> subunits;
};

/// \brief The unit tree of a NUPN. `present()` is false when the source has no
/// nupn block, in which case the importer falls back to a flat shape.
struct unit_tree {
  std::unordered_map<std::string, unit> units;
  std::string root;
  bool safe = true;

  [[nodiscard]] bool present() const { return !units.empty(); }
};

/// \brief Read the unit tree from the PNML file at \p path (its own SAX pass).
/// Returns an empty tree if there is no `tool="nupn"` toolspecific block.
[[nodiscard]] unit_tree read_units(const std::string& path);

}  // namespace hsc::petri
