/// \file translate.hh
/// \brief M2M: a `datum` AST → operations on a `core::manager`.
///
/// A separate pass from parsing (`sexpr.hh`): this is where syntax acquires
/// operational meaning. It declares leaves and a shape, compiles each event to
/// a product term, executes the commands, and reports. It is the only part of
/// the surface that depends on `hsc::core`.
///
/// Scope is the separable Presburger fragment: a crossing atom or action
/// is refused as crossing, not silently mis-compiled.
#pragma once

#include <iosfwd>
#include <map>
#include <string>

#include "hsc/surface/sexpr.hh"

namespace hsc::surface {

/// \brief A semantic error found while giving forms meaning, with a source
/// line. Distinct from `parse_error`: the syntax was well-formed, the meaning
/// was not (unknown leaf, crossing constraint, shape leaf used twice, …).
class translate_error : public std::runtime_error {
 public:
  translate_error(int line, const std::string& what)
      : std::runtime_error("line " + std::to_string(line) + ": " + what),
        line_(line) {}
  [[nodiscard]] int line() const noexcept { return line_; }

 private:
  int line_;
};

/// \brief Give \p forms meaning, writing command output to \p out.
/// \return the number of `expect` assertions that failed (0 on full success).
/// Throws `translate_error` on a malformed or out-of-scope model.
int translate(const std::vector<datum>& forms, std::ostream& out);

/// \brief Parse and translate the file at \p path. Convenience for a runner.
/// \p params overrides `param` values by name (the `-DN=…` command line).
/// \return process exit code: 0 on success, nonzero on any parse error,
/// translate error, or failed `expect`.
int run_file(const std::string& path, std::ostream& out, std::ostream& err,
             const std::map<std::string, long long>& params = {});

}  // namespace hsc::surface
