/// \file ast.hh
/// \brief The DVE model a parse produces (T2M target).
///
/// Names stay unresolved and expressions stay trees: this layer records what
/// the text says, nothing of what it means. Resolution, numbering and the
/// mapping to surface forms live in the transform (`dve_to_surface`), so
/// alternative encodings can read the same model. Knows nothing of
/// `hsc::core` or the surface.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hsc::dve {

/// Expression node kinds.
enum class ekind : std::uint8_t {
  number,      ///< an integer literal (true/false parse to 1/0)
  name,        ///< a variable or constant reference
  array_ref,   ///< NAME[expr]
  proc_state,  ///< Proc.state — "process is in this control state"
  unary,       ///< op in {not_, neg, comp}
  binary,      ///< everything else
};

/// Operators, unary and binary.
enum class eop : std::uint8_t {
  none,
  // unary
  not_, neg, comp,
  // boolean binary (one precedence tier in DVE, left-assoc)
  imply, or_, and_,
  // bitwise tier
  bor, band, bxor,
  // comparisons
  eq, ne, lt, le, gt, ge,
  // shifts, additive, multiplicative
  shl, shr, add, sub, mul, div, mod,
};

/// One expression tree node.
struct expr {
  ekind kind = ekind::number;
  eop op = eop::none;
  std::int32_t value = 0;    ///< number
  std::string name;          ///< name / array_ref / proc_state (process)
  std::string field;         ///< proc_state: the state name
  std::unique_ptr<expr> a;   ///< unary operand, binary left, array index
  std::unique_ptr<expr> b;   ///< binary right
  int line = 0;
};

/// `varaccess = expr`, one element of an effect list.
struct assign {
  std::string var;
  std::unique_ptr<expr> index;  ///< null for a scalar target
  std::unique_ptr<expr> rhs;
  int line = 0;
};

/// A transition of a process.
struct transition {
  std::string src, dest;
  std::unique_ptr<expr> guard;  ///< null: unguarded
  enum class sync_kind : std::uint8_t { none, send, recv } sync =
      sync_kind::none;
  std::string channel;
  std::unique_ptr<expr> sync_value;  ///< send: the value sent (may be null)
  std::string sync_var;              ///< recv: target variable (may be empty)
  std::unique_ptr<expr> sync_index;  ///< recv into an array cell
  std::vector<assign> effects;
  int line = 0;
};

/// A scalar or array declaration. `byte` records the opt-in [0,256) bound.
struct var_decl {
  std::string name;
  bool is_byte = false;
  bool is_array = false;
  std::int32_t size = 0;                         ///< arrays
  std::unique_ptr<expr> init;                    ///< scalars (null: 0)
  std::vector<std::unique_ptr<expr>> init_list;  ///< arrays (empty: zeros)
  int line = 0;
};

/// A compile-time constant; its value folds at transform time.
struct const_decl {
  std::string name;
  std::unique_ptr<expr> value;
  int line = 0;
};

struct process {
  std::string name;
  std::vector<var_decl> locals;
  std::vector<const_decl> constants;
  std::vector<std::string> states;  ///< numbered by this order
  std::string init_state;
  std::vector<std::string> accept, commit;  ///< kept, not yet emitted
  std::vector<std::pair<std::string, std::unique_ptr<expr>>> asserts;
  std::vector<transition> transitions;
  int line = 0;
};

struct model {
  std::vector<var_decl> globals;
  std::vector<const_decl> constants;
  std::vector<std::string> channels;
  std::vector<process> processes;
  bool async = true;  ///< `system async`; lockstep `sync` is refused later
};

/// \brief A syntax error with its source line.
class parse_error : public std::runtime_error {
 public:
  parse_error(int line, const std::string& what)
      : std::runtime_error("line " + std::to_string(line) + ": " + what),
        line_(line) {}
  [[nodiscard]] int line() const noexcept { return line_; }

 private:
  int line_;
};

/// \brief Parse DVE text into a model. Throws `parse_error`.
[[nodiscard]] model parse(const std::string& text);

}  // namespace hsc::dve
