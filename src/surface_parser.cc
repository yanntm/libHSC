/// \file surface_parser.cc
/// \brief T2M: text → `datum` AST. A hand-written recursive s-expression
/// reader — no automaton is warranted for parentheses.
///
/// The whole grammar: `datum ::= ATOM | '(' datum* ')'`, atoms are runs of
/// non-delimiter characters, `;` comments to end of line. This file has no
/// knowledge of leaves, shapes, or operations; that is a separate pass.

#include "hsc/surface/sexpr.hh"

#include <cctype>
#include <string_view>

namespace hsc::surface {

namespace {

/// A cursor over the source, tracking the current line for diagnostics.
class scanner {
 public:
  explicit scanner(std::string_view src) : src_(src) {}

  /// Skip whitespace and `;` line comments; return false at end of input.
  bool skip_trivia() {
    while (pos_ < src_.size()) {
      const char c = src_[pos_];
      if (c == '\n') {
        ++line_;
        ++pos_;
      } else if (std::isspace(static_cast<unsigned char>(c))) {
        ++pos_;
      } else if (c == ';') {
        while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
      } else {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool at_end() const noexcept { return pos_ >= src_.size(); }
  [[nodiscard]] char peek() const noexcept { return src_[pos_]; }
  [[nodiscard]] int line() const noexcept { return line_; }
  void advance() noexcept { ++pos_; }

  /// Read a maximal atom token: up to the next whitespace, paren, or comment.
  std::string atom_token() {
    const std::size_t start = pos_;
    while (pos_ < src_.size()) {
      const char c = src_[pos_];
      if (std::isspace(static_cast<unsigned char>(c)) || c == '(' ||
          c == ')' || c == ';') {
        break;
      }
      ++pos_;
    }
    return std::string(src_.substr(start, pos_ - start));
  }

 private:
  std::string_view src_;
  std::size_t pos_ = 0;
  int line_ = 1;
};

/// Read one datum. Precondition: trivia already skipped, not at end.
datum read_datum(scanner& s) {
  const int line = s.line();
  if (s.peek() == '(') {
    s.advance();  // consume '('
    std::vector<datum> items;
    for (;;) {
      if (!s.skip_trivia()) throw parse_error(line, "unterminated list");
      if (s.peek() == ')') {
        s.advance();  // consume ')'
        return datum::list(std::move(items), line);
      }
      items.push_back(read_datum(s));
    }
  }
  if (s.peek() == ')') throw parse_error(line, "unexpected ')'");
  return datum::atom(s.atom_token(), line);
}

}  // namespace

std::vector<datum> parse(std::string_view text) {
  scanner s(text);
  std::vector<datum> forms;
  while (s.skip_trivia()) forms.push_back(read_datum(s));
  return forms;
}

}  // namespace hsc::surface
