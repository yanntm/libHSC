/// \file fsp_parser.cc
/// \brief T2M for FSP: tokenize and parse the CAC08 subset
/// (`hsc/fsp/algorithm.md` §1) into the `hsc::fsp` AST.

#include <cctype>
#include <set>
#include <string_view>

#include "hsc/fsp/ast.hh"

namespace hsc::fsp {
namespace {

struct token {
  enum class kind { ident, integer, punct, eof };
  kind k = kind::eof;
  std::string text;
  long value = 0;
  int line = 1;
};

/// Tokenizer: identifiers, integers, and punctuation with max-munch on
/// the two-character operators. Comments `//` and `/* */`.
std::vector<token> lex(std::string_view s) {
  std::vector<token> out;
  int line = 1;
  std::size_t i = 0;
  const auto two = [&](char a, char b) {
    return i + 1 < s.size() && s[i] == a && s[i + 1] == b;
  };
  while (i < s.size()) {
    const char c = s[i];
    if (c == '\n') {
      ++line, ++i;
    } else if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
    } else if (two('/', '/')) {
      while (i < s.size() && s[i] != '\n') ++i;
    } else if (two('/', '*')) {
      i += 2;
      while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) {
        if (s[i] == '\n') ++line;
        ++i;
      }
      i = (i + 1 < s.size()) ? i + 2 : s.size();
    } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::size_t j = i;
      while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) ||
                              s[j] == '_'))
        ++j;
      out.push_back({token::kind::ident, std::string(s.substr(i, j - i)), 0,
                     line});
      i = j;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t j = i;
      long v = 0;
      while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j])))
        v = v * 10 + (s[j++] - '0');
      out.push_back({token::kind::integer, std::string(s.substr(i, j - i)), v,
                     line});
      i = j;
    } else {
      static const char* twos[] = {"->", "..", "<<", ">>", "<=", ">=",
                                   "==", "!=", "&&", "||"};
      std::string t(1, c);
      for (const char* p : twos)
        if (two(p[0], p[1])) {
          t = p;
          break;
        }
      out.push_back({token::kind::punct, t, 0, line});
      i += t.size();
    }
  }
  out.push_back({token::kind::eof, "", 0, line});
  return out;
}

class parser {
 public:
  explicit parser(std::string_view text) : toks_(lex(text)) {}

  model run() {
    model m;
    bool minimal = false, property = false;
    while (!at_eof()) {
      if (is_ident("const")) {
        next();
        std::string n = expect_ident("constant name");
        expect_punct("=");
        m.consts.emplace_back(n, parse_expr());
      } else if (is_ident("range")) {
        next();
        std::string n = expect_ident("range name");
        expect_punct("=");
        rng r;
        r.lo = parse_expr();
        expect_punct("..");
        r.hi = parse_expr();
        range_names_.insert(n);
        m.ranges.emplace_back(n, r);
      } else if (is_ident("minimal")) {
        next();
        minimal = true;
      } else if (is_ident("property")) {
        next();
        property = true;
      } else if (peek().k == token::kind::ident) {
        process p = parse_process();
        p.minimal = minimal;
        p.is_property = property;
        minimal = property = false;
        m.processes.push_back(std::move(p));
      } else {
        fail("unexpected token '" + peek().text + "' at top level");
      }
    }
    return m;
  }

 private:
  std::vector<token> toks_;
  std::size_t pos_ = 0;
  std::set<std::string> range_names_;

  // -- token plumbing ------------------------------------------------------
  const token& peek(std::size_t k = 0) const {
    const std::size_t i = pos_ + k;
    return i < toks_.size() ? toks_[i] : toks_.back();
  }
  bool at_eof() const { return peek().k == token::kind::eof; }
  const token& next() { return toks_[pos_++]; }
  bool is_ident(const char* s, std::size_t k = 0) const {
    return peek(k).k == token::kind::ident && peek(k).text == s;
  }
  bool is_punct(const char* s, std::size_t k = 0) const {
    return peek(k).k == token::kind::punct && peek(k).text == s;
  }
  [[noreturn]] void fail(const std::string& what) const {
    throw parse_error(peek().line, what);
  }
  std::string expect_ident(const char* what) {
    if (peek().k != token::kind::ident)
      fail(std::string("expected ") + what + ", got '" + peek().text + "'");
    return next().text;
  }
  void expect_punct(const char* s) {
    if (!is_punct(s))
      fail(std::string("expected '") + s + "', got '" + peek().text + "'");
    next();
  }

  // -- expressions (precedence climbing, algorithm.md order) ---------------
  expr parse_expr() { return parse_bin(0); }

  static int level(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "==" || op == "!=") return 3;
    if (op == "<" || op == "<=" || op == ">" || op == ">=") return 4;
    if (op == "<<" || op == ">>") return 5;
    if (op == "+" || op == "-") return 6;
    if (op == "*" || op == "/" || op == "%") return 7;
    return 0;
  }

  expr parse_bin(int min_level) {
    expr lhs = parse_unary();
    while (peek().k == token::kind::punct) {
      const int l = level(peek().text);
      if (l == 0 || l < min_level) break;
      expr e;
      e.k = expr::kind::binary;
      e.op = next().text;
      e.args.push_back(std::move(lhs));
      e.args.push_back(parse_bin(l + 1));
      lhs = std::move(e);
    }
    return lhs;
  }

  expr parse_unary() {
    if (is_punct("!") || is_punct("-")) {
      expr e;
      e.k = expr::kind::unary;
      e.op = next().text;
      e.args.push_back(parse_unary());
      return e;
    }
    if (is_punct("(")) {
      next();
      expr e = parse_expr();
      expect_punct(")");
      return e;
    }
    expr e;
    if (peek().k == token::kind::integer) {
      e.k = expr::kind::integer;
      e.value = next().value;
    } else if (peek().k == token::kind::ident) {
      e.k = expr::kind::name;
      e.name = next().text;
    } else {
      fail("expected an expression, got '" + peek().text + "'");
    }
    return e;
  }

  // -- ranges and label patterns ------------------------------------------
  /// After a `:`, or standalone in `[…]`: `NAME` (declared range) or
  /// `expr .. expr`.
  rng parse_rng() {
    expr e = parse_expr();
    if (is_punct("..")) {
      next();
      rng r;
      r.lo = std::move(e);
      r.hi = parse_expr();
      return r;
    }
    if (e.k != expr::kind::name)
      fail("expected a range (NAME or lo..hi)");
    rng r;
    r.name = e.name;
    return r;
  }

  /// The bracketed element forms: binder, anonymous choice, or index.
  label_elem parse_bracket() {
    const int line = peek().line;
    expect_punct("[");
    label_elem el;
    if (peek().k == token::kind::ident && is_punct(":", 1)) {
      el.k = label_elem::kind::binder;
      el.var = next().text;
      next();  // ':'
      el.range = parse_rng();
    } else {
      expr e = parse_expr();
      if (is_punct("..")) {
        next();
        el.k = label_elem::kind::choice;
        el.range.lo = std::move(e);
        el.range.hi = parse_expr();
      } else if (e.k == expr::kind::name && range_names_.count(e.name)) {
        el.k = label_elem::kind::choice;
        el.range.name = e.name;
      } else {
        el.k = label_elem::kind::index;
        el.ix = std::move(e);
      }
    }
    (void)line;
    expect_punct("]");
    return el;
  }

  label_elem parse_elem() {
    if (peek().k == token::kind::ident) {
      label_elem el;
      el.k = label_elem::kind::name;
      el.text = next().text;
      return el;
    }
    if (is_punct("[")) return parse_bracket();
    if (is_punct("{")) {
      next();
      label_elem el;
      el.k = label_elem::kind::set;
      el.alts.push_back(parse_label());
      while (is_punct(",")) {
        next();
        el.alts.push_back(parse_label());
      }
      expect_punct("}");
      return el;
    }
    fail("expected a label element, got '" + peek().text + "'");
  }

  /// A label pattern: elements chained by `[`, `{`, or `.`-then-element.
  label_pattern parse_label() {
    label_pattern p;
    p.line = peek().line;
    p.elems.push_back(parse_elem());
    for (;;) {
      if (is_punct("[") || is_punct("{")) {
        p.elems.push_back(parse_elem());
      } else if (is_punct(".") && (peek(1).k == token::kind::ident ||
                                   is_punct("[", 1) || is_punct("{", 1))) {
        next();
        p.elems.push_back(parse_elem());
      } else {
        return p;
      }
    }
  }

  // -- branches, bodies, processes ----------------------------------------
  /// The last pattern of a `->` chain is the target: NAME + index elems,
  /// or `ERROR`.
  state_ref to_target(const label_pattern& p) {
    state_ref r;
    r.line = p.line;
    if (p.elems.empty() || p.elems.front().k != label_elem::kind::name)
      throw parse_error(p.line, "malformed target state reference");
    r.name = p.elems.front().text;
    if (r.name == "ERROR") {
      if (p.elems.size() != 1)
        throw parse_error(p.line, "ERROR takes no arguments");
      r.is_error = true;
      return r;
    }
    for (std::size_t i = 1; i < p.elems.size(); ++i) {
      if (p.elems[i].k != label_elem::kind::index)
        throw parse_error(
            p.line, "target argument of '" + r.name + "' is not an expression");
      r.args.push_back(*p.elems[i].ix);
    }
    return r;
  }

  branch parse_branch() {
    branch b;
    b.line = peek().line;
    if (is_ident("when")) {
      next();
      expect_punct("(");
      b.guard = parse_expr();
      expect_punct(")");
    }
    b.labels.push_back(parse_label());
    while (is_punct("->")) {
      next();
      b.labels.push_back(parse_label());
    }
    if (b.labels.size() < 2)
      fail("a branch needs at least 'label -> target'");
    b.target = to_target(b.labels.back());
    b.labels.pop_back();
    return b;
  }

  std::vector<branch> parse_body() {
    if (is_ident("STOP")) {  // a deadlocked state: no branches
      next();
      return {};
    }
    expect_punct("(");
    std::vector<branch> body;
    body.push_back(parse_branch());
    while (is_punct("|")) {
      next();
      body.push_back(parse_branch());
    }
    expect_punct(")");
    return body;
  }

  /// Trailing `+{…}` / `\{…}` after any body; they scope to the whole
  /// process definition.
  void parse_extensions(process& p) {
    for (;;) {
      const bool ext = is_punct("+") && is_punct("{", 1);
      const bool hide = is_punct("\\");
      if (!ext && !hide) return;
      next();  // '+' or '\'
      expect_punct("{");
      auto& dst = ext ? p.extension : p.hidden;
      dst.push_back(parse_label());
      while (is_punct(",")) {
        next();
        dst.push_back(parse_label());
      }
      expect_punct("}");
    }
  }

  state_ref parse_state_ref() {
    state_ref r;
    r.line = peek().line;
    r.name = expect_ident("state name");
    while (is_punct("[")) {
      next();
      r.args.push_back(parse_expr());
      expect_punct("]");
    }
    return r;
  }

  process parse_process() {
    process p;
    p.line = peek().line;
    p.name = expect_ident("process name");
    expect_punct("=");
    if (is_punct("(")) {
      p.init_body = parse_body();
    } else {
      p.init_ref = parse_state_ref();
    }
    parse_extensions(p);
    while (is_punct(",")) {
      next();
      state_def d;
      d.line = peek().line;
      d.name = expect_ident("state definition name");
      while (is_punct("[")) {
        next();
        std::string v = expect_ident("state parameter");
        expect_punct(":");
        d.params.emplace_back(std::move(v), parse_rng());
        expect_punct("]");
      }
      expect_punct("=");
      d.body = parse_body();
      p.states.push_back(std::move(d));
      parse_extensions(p);
    }
    parse_extensions(p);
    expect_punct(".");
    return p;
  }
};

}  // namespace

model parse(std::string_view text) { return parser(text).run(); }

}  // namespace hsc::fsp
