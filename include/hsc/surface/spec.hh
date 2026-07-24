/// \file spec.hh
/// \brief The declarations of a binder-free `.hsc` as plain data, and
/// analyses over them.
///
/// An algorithm on the datum structure: no translator, no diagrams. `read`
/// walks the declaration forms — leaves, arrays, the shape (flattened to
/// the frontier), init, events, families (instantiated) — and the result
/// serves as the `name_scope` an `expr_reader` resolves through. Analyses
/// (domain inference today) build the explicit model from it and run.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "hsc/surface/xpl_build.hh"
#include "hsc/xpl/domains.hh"

namespace hsc::surface {

/// The declared side of a spec, read once from the forms.
class spec final : public name_scope {
 public:
  /// Read the declaration forms of \p forms (command forms are ignored).
  /// Binder-free input assumed; throws `translate_error` on malformed
  /// declarations.
  [[nodiscard]] static spec read(const std::vector<datum>& forms);

  /// \name name_scope
  ///@{
  [[nodiscard]] std::optional<std::uint32_t> position(
      const std::string& name) const override;
  [[nodiscard]] std::optional<std::vector<std::uint32_t>> array(
      const std::string& name) const override;
  ///@}

  /// Leaf names in frontier order (the flattened shape).
  [[nodiscard]] const std::vector<std::string>& order() const {
    return order_;
  }
  /// Declared bound of the leaf at \p pos, as [lo, hi); nullopt if none.
  [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>> bound(
      std::uint32_t pos) const;
  /// Declared arrays: name → cell positions in index order.
  [[nodiscard]] const std::map<std::string, std::vector<std::uint32_t>>&
  arrays() const {
    return arrays_;
  }
  /// The events (families instantiated), as the explicit builder eats them.
  [[nodiscard]] const std::vector<xpl_source>& events() const {
    return events_;
  }

  /// The initial states: the base word (defaults edited by pair inits),
  /// or, when an init *event* is declared, its image by one explicit
  /// firing. Throws when the init event is beyond the supported clause
  /// forms or has an empty image.
  [[nodiscard]] std::vector<xpl::word> seeds(lia::expr_factory& ex) const;

 private:
  struct leaf_info {
    std::uint32_t pos = 0;
    bool bounded = false;
    std::int32_t lo = 0;
    std::int32_t hi = 0;
  };
  std::map<std::string, leaf_info> leaves_;
  std::vector<std::string> order_;
  std::map<std::string, std::vector<std::uint32_t>> arrays_;
  std::vector<xpl_source> events_;
  std::vector<std::pair<std::uint32_t, std::int32_t>> init_pairs_;
  std::vector<datum> init_event_;  ///< the clause forms, when init is one
};

/// One inference unit — a scalar leaf or a whole array — with its
/// inferred domain and its declared bound: `(xdomains)` as data.
struct unit_domain {
  std::string name;
  bool is_array = false;
  bool declared = false;  ///< a [lo, hi) bound was declared
  std::int32_t decl_lo = 0;
  std::int32_t decl_hi = 0;
  xpl::domain_report report;
};

/// Domain inference over the declarations of \p forms
/// (`xpl/domains.hh`). Empty when no shape is declared.
[[nodiscard]] std::vector<unit_domain> analyze_domains(
    const std::vector<datum>& forms);

/// Print \p units as `(xdomains)` does: one `xdom` line per unit, then
/// an `xdomains` summary line.
void print_domains(std::ostream& os, const std::vector<unit_domain>& units);

}  // namespace hsc::surface
