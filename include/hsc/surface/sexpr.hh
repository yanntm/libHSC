/// \file sexpr.hh
/// \brief The AST of the file surface, and the parser that builds it (T2M).
///
/// An s-expression language is homoiconic: the parse tree *is* the AST, so no
/// separate typed tree is minted at this layer. A `datum` is an atom or a list;
/// it carries a source line so a later pass can report errors in the user's
/// terms. This header knows nothing of `hsc::core` — parsing syntax to an AST
/// is deliberately not confused with giving it operational meaning (that is
/// `translate.hh`). Many passes may one day sit between the two.
#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace hsc::surface {

/// \brief One node of the syntax tree: an atom (symbol or integer token) or a
/// list of nested data. Atomhood is `list_.empty() && !is_list_`.
class datum {
 public:
  static datum atom(std::string text, int line) {
    datum d;
    d.is_list_ = false;
    d.text_ = std::move(text);
    d.line_ = line;
    return d;
  }
  static datum list(std::vector<datum> items, int line) {
    datum d;
    d.is_list_ = true;
    d.items_ = std::move(items);
    d.line_ = line;
    return d;
  }

  [[nodiscard]] bool is_atom() const noexcept { return !is_list_; }
  [[nodiscard]] bool is_list() const noexcept { return is_list_; }
  [[nodiscard]] int line() const noexcept { return line_; }

  /// The token text of an atom. Empty for a list.
  [[nodiscard]] const std::string& text() const noexcept { return text_; }
  /// The children of a list. Empty for an atom.
  [[nodiscard]] const std::vector<datum>& items() const noexcept {
    return items_;
  }

  /// \brief The head symbol of a list `(head …)`, or "" if empty / not a list.
  /// A convenience for the translator's keyword dispatch.
  [[nodiscard]] const std::string& head() const noexcept {
    static const std::string empty;
    if (!is_list_ || items_.empty() || !items_.front().is_atom()) return empty;
    return items_.front().text_;
  }

 private:
  bool is_list_ = false;
  std::string text_;
  std::vector<datum> items_;
  int line_ = 0;
};

/// \brief A syntax error, with the 1-based line it was found on.
class parse_error : public std::runtime_error {
 public:
  parse_error(int line, const std::string& what)
      : std::runtime_error("line " + std::to_string(line) + ": " + what),
        line_(line) {}
  [[nodiscard]] int line() const noexcept { return line_; }

 private:
  int line_;
};

/// \brief Parse the top-level forms of \p text. Throws `parse_error` on
/// unbalanced parentheses. Comments run from `;` to end of line.
[[nodiscard]] std::vector<datum> parse(std::string_view text);

/// \brief Render \p d back to s-expression text, one datum, no newline.
/// `write(parse(t))` round-trips modulo whitespace and comments.
void write(std::ostream& os, const datum& d);

}  // namespace hsc::surface
