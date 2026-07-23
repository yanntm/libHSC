/// \file expr.hh
/// \brief Surface expression forms → `lia` codes over frontier positions.
///
/// The reader turns a `datum` tree into an interned `lia` expression,
/// resolving names through a scope its host provides. It is the whole of
/// the surface's expression syntax:
///
///     EXPR ::= INT | NAME | (at NAME EXPR) | (OP EXPR EXPR) | (~ EXPR)
///     OP   ::= + - * / % << >> & | ^
///     BEXP ::= (CMP EXPR EXPR) | (in EXPR INT+) | (and BEXP BEXP)
///            | (or BEXP BEXP) | (not BEXP) | (imply BEXP BEXP)
///     CMP  ::= == != < <= > >=
///
/// The two kinds coerce both ways, as in GAL: a boolean form in integer
/// position reads as 0/1 (`wrap`), an integer form in boolean position
/// reads `!= 0`. `at` uses the array convention of `lia`: the id is the
/// frontier position of cell 0, so a constant index resolves to a plain
/// position and an out-of-range one to ⊥.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hsc/lia/expr.hh"
#include "hsc/surface/sexpr.hh"

namespace hsc::surface {

/// Name resolution the reader needs from its host.
class name_scope {
 public:
  virtual ~name_scope() = default;
  /// The frontier position of scalar leaf \p name, if declared.
  [[nodiscard]] virtual std::optional<std::uint32_t> position(
      const std::string& name) const = 0;
  /// The frontier position of each cell of array \p name, in index order,
  /// if declared. Cells sit wherever the shape put them: no adjacency.
  [[nodiscard]] virtual std::optional<std::vector<std::uint32_t>> array(
      const std::string& name) const = 0;
};

/// \brief `datum` → `lia`, throwing `translate_error` on an unknown name or
/// form. Stateless beyond its two references.
class expr_reader {
 public:
  expr_reader(lia::expr_factory& ex, const name_scope& scope)
      : ex_(ex), scope_(scope) {}

  [[nodiscard]] lia::iexpr read_int(const datum& d) const;
  [[nodiscard]] lia::bexpr read_bool(const datum& d) const;

 private:
  lia::expr_factory& ex_;
  const name_scope& scope_;
};

}  // namespace hsc::surface
