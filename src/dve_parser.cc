/// \file dve_parser.cc
/// \brief T2M: DVE text → `dve::model`. Hand lexer and recursive descent
/// after `Divine.xtext`; precedence tiers are DiVinE's own.

#include <cctype>
#include <charconv>
#include <string>
#include <vector>

#include "hsc/dve/ast.hh"

namespace hsc::dve {

namespace {

enum class tk : std::uint8_t { id, num, punct, end };

struct token {
  tk kind = tk::end;
  std::string text;
  std::int32_t num = 0;
  int line = 1;
};

/// Longest-match lexer; `//` and `/* */` comments; multi-char punctuation.
class lexer {
 public:
  explicit lexer(const std::string& text) : s_(text) { advance(); }

  const token& peek() const { return cur_; }
  token take() {
    token t = cur_;
    advance();
    return t;
  }

 private:
  void skip_space() {
    for (;;) {
      while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) {
        if (s_[pos_] == '\n') ++line_;
        ++pos_;
      }
      if (pos_ + 1 < s_.size() && s_[pos_] == '/' && s_[pos_ + 1] == '/') {
        while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_;
        continue;
      }
      if (pos_ + 1 < s_.size() && s_[pos_] == '/' && s_[pos_ + 1] == '*') {
        pos_ += 2;
        while (pos_ + 1 < s_.size() &&
               !(s_[pos_] == '*' && s_[pos_ + 1] == '/')) {
          if (s_[pos_] == '\n') ++line_;
          ++pos_;
        }
        if (pos_ + 1 >= s_.size()) throw parse_error(line_, "unclosed comment");
        pos_ += 2;
        continue;
      }
      return;
    }
  }

  void advance() {
    skip_space();
    cur_.line = line_;
    if (pos_ >= s_.size()) {
      cur_ = {tk::end, "", 0, line_};
      return;
    }
    const char c = s_[pos_];
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::size_t b = pos_;
      while (pos_ < s_.size() &&
             (std::isalnum(static_cast<unsigned char>(s_[pos_])) ||
              s_[pos_] == '_')) {
        ++pos_;
      }
      cur_ = {tk::id, s_.substr(b, pos_ - b), 0, line_};
      return;
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t b = pos_;
      while (pos_ < s_.size() &&
             std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
        ++pos_;
      }
      std::int32_t v = 0;
      std::from_chars(s_.data() + b, s_.data() + pos_, v);
      cur_ = {tk::num, s_.substr(b, pos_ - b), v, line_};
      return;
    }
    // punctuation, longest first
    static constexpr const char* two[] = {"=>", "||", "&&", "==", "!=", "<=",
                                          ">=", "<<", ">>", "->"};
    for (const char* p : two) {
      if (pos_ + 1 < s_.size() && s_[pos_] == p[0] && s_[pos_ + 1] == p[1]) {
        cur_ = {tk::punct, p, 0, line_};
        pos_ += 2;
        return;
      }
    }
    if (c == '"') throw parse_error(line_, "string literals are not supported");
    cur_ = {tk::punct, std::string(1, c), 0, line_};
    ++pos_;
  }

  const std::string& s_;
  std::size_t pos_ = 0;
  int line_ = 1;
  token cur_;
};

class parser {
 public:
  explicit parser(const std::string& text) : lx_(text) {}

  model run() {
    model m;
    for (;;) {
      const token& t = lx_.peek();
      if (t.kind != tk::id) fail("expected a declaration or 'system'");
      if (t.text == "const") parse_consts(m.constants);
      else if (t.text == "int" || t.text == "byte") parse_vars(m.globals);
      else if (t.text == "channel") parse_channels(m);
      else if (t.text == "process") m.processes.push_back(parse_process());
      else if (t.text == "system") break;
      else fail("unexpected '" + t.text + "'");
    }
    expect_id("system");
    const token st = lx_.take();
    if (st.kind != tk::id || (st.text != "async" && st.text != "sync")) {
      fail("system is 'async' or 'sync'");
    }
    m.async = st.text == "async";
    expect(";");
    if (lx_.peek().kind != tk::end) fail("trailing input after 'system'");
    return m;
  }

 private:
  [[noreturn]] void fail(const std::string& msg) const {
    throw parse_error(lx_.peek().line, msg);
  }

  void expect(const std::string& p) {
    const token t = lx_.take();
    if (t.kind != tk::punct || t.text != p) {
      throw parse_error(t.line, "expected '" + p + "', found '" + t.text + "'");
    }
  }
  void expect_id(const std::string& kw) {
    const token t = lx_.take();
    if (t.kind != tk::id || t.text != kw) {
      throw parse_error(t.line,
                        "expected '" + kw + "', found '" + t.text + "'");
    }
  }
  bool eat(const std::string& p) {
    if (lx_.peek().kind == tk::punct && lx_.peek().text == p) {
      lx_.take();
      return true;
    }
    return false;
  }
  bool eat_id(const std::string& kw) {
    if (lx_.peek().kind == tk::id && lx_.peek().text == kw) {
      lx_.take();
      return true;
    }
    return false;
  }
  std::string ident(const char* what) {
    const token t = lx_.take();
    if (t.kind != tk::id) {
      throw parse_error(t.line, std::string("expected ") + what);
    }
    return t.text;
  }

  // --- declarations --------------------------------------------------------

  void parse_consts(std::vector<const_decl>& out) {
    expect_id("const");
    const token ty = lx_.take();  // int | byte, irrelevant to a constant
    if (ty.kind != tk::id || (ty.text != "int" && ty.text != "byte")) {
      throw parse_error(ty.line, "const takes a type, int or byte");
    }
    do {
      const_decl c;
      c.line = lx_.peek().line;
      c.name = ident("a constant name");
      expect("=");
      c.value = parse_expr();
      out.push_back(std::move(c));
    } while (eat(","));
    expect(";");
  }

  void parse_vars(std::vector<var_decl>& out) {
    const token ty = lx_.take();
    const bool is_byte = ty.text == "byte";
    do {
      var_decl v;
      v.line = lx_.peek().line;
      v.is_byte = is_byte;
      v.name = ident("a variable name");
      if (eat("[")) {
        v.is_array = true;
        const token n = lx_.take();
        if (n.kind != tk::num) throw parse_error(n.line, "array size");
        v.size = n.num;
        expect("]");
        if (eat("=")) {
          expect("{");
          do v.init_list.push_back(parse_expr());
          while (eat(","));
          expect("}");
        }
      } else if (eat("=")) {
        v.init = parse_expr();
      }
      out.push_back(std::move(v));
    } while (eat(","));
    expect(";");
  }

  void parse_channels(model& m) {
    expect_id("channel");
    do m.channels.push_back(ident("a channel name"));
    while (eat(","));
    expect(";");
  }

  // --- processes -----------------------------------------------------------

  process parse_process() {
    process p;
    p.line = lx_.peek().line;
    expect_id("process");
    p.name = ident("a process name");
    expect("{");
    for (;;) {
      if (lx_.peek().kind != tk::id) break;
      const std::string& kw = lx_.peek().text;
      if (kw == "const") parse_consts(p.constants);
      else if (kw == "int" || kw == "byte") parse_vars(p.locals);
      else break;
    }
    if (eat_id("state")) {
      do p.states.push_back(ident("a state name"));
      while (eat(","));
      expect(";");
      expect_id("init");
      p.init_state = ident("the init state");
      expect(";");
      if (eat_id("accept")) {
        do p.accept.push_back(ident("a state name"));
        while (eat(","));
        expect(";");
      }
      if (eat_id("commit")) {
        do p.commit.push_back(ident("a state name"));
        while (eat(","));
        expect(";");
      }
    }
    if (eat_id("assert")) {
      do {
        std::string st = ident("a state name");
        expect(":");
        p.asserts.emplace_back(std::move(st), parse_expr());
      } while (eat(","));
      expect(";");
    }
    expect_id("trans");
    do p.transitions.push_back(parse_transition());
    while (eat(","));
    expect(";");
    expect("}");
    return p;
  }

  transition parse_transition() {
    transition t;
    t.line = lx_.peek().line;
    t.src = ident("a source state");
    expect("->");
    t.dest = ident("a target state");
    expect("{");
    if (eat_id("guard")) {
      t.guard = parse_expr();
      expect(";");
    }
    if (eat_id("sync")) {
      t.channel = ident("a channel name");
      if (eat("!")) {
        t.sync = transition::sync_kind::send;
        if (!(lx_.peek().kind == tk::punct && lx_.peek().text == ";")) {
          t.sync_value = parse_expr();
        }
      } else {
        expect("?");
        t.sync = transition::sync_kind::recv;
        if (lx_.peek().kind == tk::id) {
          t.sync_var = ident("a variable");
          if (eat("[")) {
            t.sync_index = parse_expr();
            expect("]");
          }
        }
      }
      expect(";");
    }
    if (eat_id("effect")) {
      do {
        assign a;
        a.line = lx_.peek().line;
        a.var = ident("an assignment target");
        if (eat("[")) {
          a.index = parse_expr();
          expect("]");
        }
        expect("=");
        a.rhs = parse_expr();
        t.effects.push_back(std::move(a));
      } while (eat(","));
      expect(";");
    }
    expect("}");
    return t;
  }

  // --- expressions: DiVinE's precedence tiers ------------------------------

  std::unique_ptr<expr> mk(ekind k, int line) {
    auto e = std::make_unique<expr>();
    e->kind = k;
    e->line = line;
    return e;
  }

  std::unique_ptr<expr> binary(eop op, std::unique_ptr<expr> l,
                               std::unique_ptr<expr> r, int line) {
    auto e = mk(ekind::binary, line);
    e->op = op;
    e->a = std::move(l);
    e->b = std::move(r);
    return e;
  }

  eop peek_op(int tier) {
    const token& t = lx_.peek();
    const std::string& x = t.text;
    switch (tier) {
      case 0:  // boolean connectives, one tier in DVE
        if (t.kind == tk::punct) {
          if (x == "=>") return eop::imply;
          if (x == "||") return eop::or_;
          if (x == "&&") return eop::and_;
        } else if (t.kind == tk::id) {
          if (x == "imply") return eop::imply;
          if (x == "or") return eop::or_;
          if (x == "and") return eop::and_;
        }
        return eop::none;
      case 1:
        if (t.kind != tk::punct) return eop::none;
        if (x == "|") return eop::bor;
        if (x == "&") return eop::band;
        if (x == "^") return eop::bxor;
        return eop::none;
      case 2:
        if (t.kind != tk::punct) return eop::none;
        if (x == "==") return eop::eq;
        if (x == "!=") return eop::ne;
        if (x == "<") return eop::lt;
        if (x == "<=") return eop::le;
        if (x == ">") return eop::gt;
        if (x == ">=") return eop::ge;
        return eop::none;
      case 3:
        if (t.kind != tk::punct) return eop::none;
        if (x == "<<") return eop::shl;
        if (x == ">>") return eop::shr;
        return eop::none;
      case 4:
        if (t.kind != tk::punct) return eop::none;
        if (x == "+") return eop::add;
        if (x == "-") return eop::sub;
        return eop::none;
      case 5:
        if (t.kind != tk::punct) return eop::none;
        if (x == "*") return eop::mul;
        if (x == "/") return eop::div;
        if (x == "%") return eop::mod;
        return eop::none;
      default:
        return eop::none;
    }
  }

  std::unique_ptr<expr> parse_expr() { return parse_tier(0); }

  std::unique_ptr<expr> parse_tier(int tier) {
    if (tier == 6) return parse_prefixed();
    auto l = parse_tier(tier + 1);
    for (;;) {
      const eop op = peek_op(tier);
      if (op == eop::none) return l;
      const int line = lx_.take().line;
      l = binary(op, std::move(l), parse_tier(tier + 1), line);
    }
  }

  std::unique_ptr<expr> parse_prefixed() {
    const token& t = lx_.peek();
    const bool word_not = t.kind == tk::id && t.text == "not";
    if (word_not || (t.kind == tk::punct &&
                     (t.text == "!" || t.text == "-" || t.text == "~"))) {
      const token op = lx_.take();
      auto e = mk(ekind::unary, op.line);
      e->op = op.text == "-" ? eop::neg : op.text == "~" ? eop::comp : eop::not_;
      e->a = parse_prefixed();  // right-associative
      return e;
    }
    return parse_atom();
  }

  std::unique_ptr<expr> parse_atom() {
    const token t = lx_.take();
    if (t.kind == tk::punct && t.text == "(") {
      auto e = parse_expr();
      expect(")");
      return e;
    }
    if (t.kind == tk::num) {
      auto e = mk(ekind::number, t.line);
      e->value = t.num;
      return e;
    }
    if (t.kind == tk::id) {
      if (t.text == "true" || t.text == "false") {
        auto e = mk(ekind::number, t.line);
        e->value = t.text == "true" ? 1 : 0;
        return e;
      }
      if (eat("[")) {
        auto e = mk(ekind::array_ref, t.line);
        e->name = t.text;
        e->a = parse_expr();
        expect("]");
        return e;
      }
      if (eat(".")) {
        auto e = mk(ekind::proc_state, t.line);
        e->name = t.text;
        e->field = ident("a state name");
        return e;
      }
      auto e = mk(ekind::name, t.line);
      e->name = t.text;
      return e;
    }
    throw parse_error(t.line, "expected an expression, found '" + t.text + "'");
  }

  lexer lx_;
};

}  // namespace

model parse(const std::string& text) { return parser(text).run(); }

}  // namespace hsc::dve
