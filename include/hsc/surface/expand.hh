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

#include <map>
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
///
/// With \p families (the default), an event whose whole body is a single
/// `exists` binder satisfying the uniformity certificate (spec Part II §6:
/// index used only as an array index at a constant offset mod N, full
/// range, one binder) is NOT enumerated: it is emitted as a
/// `(family NAME N CLAUSE…)` form, accesses rewritten to `(at@ ARRAY δ)`,
/// for the translator to build by recursion instead of by instance.
/// `families = false` (the `--expand` dump) enumerates everything.
///
/// \p overrides substitutes a `param`'s declared value by name (the
/// `-DN=…` command line): the file's expression is ignored for an
/// overridden name; overriding an undeclared param is an error.
[[nodiscard]] std::vector<datum> expand(
    const std::vector<datum>& forms, bool families = true,
    const std::map<std::string, long long>& overrides = {});

}  // namespace hsc::surface
