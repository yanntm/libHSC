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
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

/// \brief One command-line argument of a session: a `.hsc` file, or inline
/// `.hsc` text (`-e`). Order is invocation order.
struct session_arg {
  bool is_file;      ///< file path (spliced as `(input PATH)`) vs inline text
  std::string text;  ///< the path, or the inline forms
};

/// \brief Run the concatenation of \p args as one session, in order. The
/// invocation grammar is sugar over `input`: a file argument behaves
/// exactly as `(input PATH)` at that position — relative paths resolve
/// against the current directory — and an inline argument as its parsed
/// forms. \return process exit code as `run_file`.
int run_session(const std::vector<session_arg>& args, std::ostream& out,
                std::ostream& err,
                const std::map<std::string, long long>& params = {});

/// \brief An incremental session: forms fed in batches to one translator,
/// results persisting between batches. For a driver that decides its next
/// query from the answer to the previous one (a search over bounds) without
/// recomputing the model. Rewrite directives act on the batch that carries
/// them, so the model and its directives go in the first batch.
class session {
 public:
  explicit session(std::ostream& out);
  ~session();
  session(const session&) = delete;
  session& operator=(const session&) = delete;
  /// Give \p forms meaning, writing command output to the session's stream.
  /// \return the number of `expect` assertions failed so far.
  int feed(const std::vector<datum>& forms);
  /// \brief A deadline for the forms fed next: the predicate is consulted
  /// once per iteration round of the calculus and a computation it stops
  /// reports as interrupted (a `ctl` form answers `TIMEOUT`, its memoised
  /// partial results kept for a later batch). Empty clears it.
  void set_interrupt(std::function<bool()> hook);

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace hsc::surface
