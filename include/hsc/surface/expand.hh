/// \file expand.hh
/// \brief The parametric pass: `param`, count-form `array`, and the binders
/// `forall` / `exists`, as a datum → datum rewrite between parser and
/// translator.
///
/// A binder carries no semantics of its own: it generates a list in place,
/// and the syntactic position of that list decides its meaning — `forall`
/// expands through the context's conjunctive combinator (seq / and / clause
/// splice), `exists` through the disjunctive one (alt / or / an event
/// family). The pass reads keywords like the translator but drives no
/// engine; it is the identity on binder-free input. Grammar and the
/// context → combinator table: research_notes/hsc_parametric.md.
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "hsc/surface/sexpr.hh"

namespace hsc::surface {

/// \brief An error found while expanding parametric forms, with a source
/// line. The syntax was well-formed s-expressions; the parametric structure
/// was not (unbound or shadowed name, non-constant range, binder in a
/// position that has no combinator, …).
class expand_error : public std::runtime_error {
 public:
  expand_error(int line, const std::string& what)
      : std::runtime_error("line " + std::to_string(line) + ": " + what),
        line_(line) {}
  [[nodiscard]] int line() const noexcept { return line_; }

 private:
  int line_;
};

/// \brief Expand every parametric form of \p forms. The result contains no
/// `param`, no count-form `array`, no `forall` / `exists`; integer
/// arithmetic over constants is folded, and a grounded access to a
/// generated array is rewritten to its cell name. Throws `expand_error`.
[[nodiscard]] std::vector<datum> expand(const std::vector<datum>& forms);

}  // namespace hsc::surface
