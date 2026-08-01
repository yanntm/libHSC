/// \file certcheck.hh
/// \brief The trusted core: check a certificate document against the
/// current spec.
///
/// Obligations: one inclusion walk per distinct leaf (leaf traces
/// inside the classifier), plus the inductive-invariant scan (G1–G3) on
/// the `inv` set. This unit shares the parser and the fragment bridge
/// with the rest of the system — one grammar, one parser — and none of
/// the loop's logic: no search, no learner, no canonicalizer.
#pragma once

#include <iosfwd>
#include <vector>

#include "hsc/surface/cegar_build.hh"

namespace hsc::surface {

/// Check the certificate given by \p cert_forms against \p s. Prints
/// one line per obligation to \p out. Returns the number of failed
/// obligations (0 = the certificate proves the property on this spec).
[[nodiscard]] int certcheck(const spec& s, lia::expr_factory& ex,
                            const expr_reader& reader,
                            const std::vector<datum>& cert_forms,
                            std::ostream& out);

}  // namespace hsc::surface
