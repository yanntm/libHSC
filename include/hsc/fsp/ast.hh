/// \file ast.hh
/// \brief T2M: the FSP (LTSA) model as parsed — the CAC08 subset of
/// `algorithm.md` §1, and the parser that builds it.
///
/// Names stay unresolved and expressions stay trees; nothing is numbered or
/// evaluated at this layer. Grounding (evaluation to a per-process LTS) is
/// `ground.hh`.
#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace hsc::fsp {

/// \brief An expression tree. `op` is the operator token (`+`, `<<`, `&&`,
/// `!`, …); an atom is `INT` (in `value`) or `NAME` (in `name`).
struct expr {
  enum class kind { integer, name, unary, binary };
  kind k = kind::integer;
  long value = 0;                     ///< kind::integer
  std::string name;                   ///< kind::name
  std::string op;                     ///< unary / binary operator token
  std::vector<expr> args;             ///< 1 (unary) or 2 (binary) children
};

/// \brief A range: a named `range` declaration, or inline `lo..hi`
/// (inclusive bounds, FSP convention).
struct rng {
  std::string name;                   ///< non-empty: a declared range
  std::optional<expr> lo, hi;         ///< inline bounds otherwise
};

struct label_pattern;

/// \brief One element of a label pattern (`algorithm.md` §1 `lelem`).
struct label_elem {
  enum class kind {
    name,     ///< a bare identifier atom
    index,    ///< `[expr]` — a computed atom
    choice,   ///< `[rng]` — anonymous choice over a range
    binder,   ///< `[v:rng]` — choice binding `var` for the rest of the branch
    set       ///< `{pat, …}` — choice over sub-patterns
  };
  kind k = kind::name;
  std::string text;                   ///< kind::name — the identifier
  std::optional<expr> ix;             ///< kind::index
  std::string var;                    ///< kind::binder — the bound name
  rng range;                          ///< kind::choice / kind::binder
  std::vector<label_pattern> alts;    ///< kind::set
};

/// \brief A label pattern: a sequence of elements; `.` and `[…]` both
/// concatenate.
struct label_pattern {
  std::vector<label_elem> elems;
  int line = 0;
};

/// \brief A state reference `NAME[expr]…` (target or initial).
struct state_ref {
  std::string name;
  std::vector<expr> args;
  bool is_error = false;              ///< the designated `ERROR` sink
  int line = 0;
};

/// \brief One branch of a choice body: optional guard, a chain of labels,
/// the target.
struct branch {
  std::optional<expr> guard;          ///< the `when (…)` condition
  std::vector<label_pattern> labels;  ///< the prefix chain, ≥ 1
  state_ref target;
  int line = 0;
};

/// \brief One state definition `NAME[v:rng]… = (branch | …)`.
struct state_def {
  std::string name;
  std::vector<std::pair<std::string, rng>> params;
  std::vector<branch> body;
  int line = 0;
};

/// \brief One process definition.
struct process {
  std::string name;
  bool is_property = false;           ///< preceded by `property`
  bool minimal = false;               ///< preceded by `minimal` (ignored)
  /// The initial local expression: a reference, or an inline anonymous
  /// body (`P = (…)`) recorded as `init_body`.
  std::optional<state_ref> init_ref;
  std::vector<branch> init_body;
  std::vector<state_def> states;
  std::vector<label_pattern> extension;  ///< `+{…}`, whole definition
  std::vector<label_pattern> hidden;     ///< `\{…}`, whole definition
  int line = 0;
};

/// \brief A parsed `.lts` file.
struct model {
  std::vector<std::pair<std::string, expr>> consts;  ///< in file order
  std::vector<std::pair<std::string, rng>> ranges;
  std::vector<process> processes;
};

/// \brief A syntax error (or a construct outside the CAC08 subset), with
/// its 1-based line.
class parse_error : public std::runtime_error {
 public:
  parse_error(int line, const std::string& what)
      : std::runtime_error("line " + std::to_string(line) + ": " + what),
        line_(line) {}
  [[nodiscard]] int line() const noexcept { return line_; }

 private:
  int line_;
};

/// \brief Parse \p text as an `.lts` file. Throws `parse_error`.
[[nodiscard]] model parse(std::string_view text);

}  // namespace hsc::fsp
