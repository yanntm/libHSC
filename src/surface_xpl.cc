/// \file surface_xpl.cc
/// \brief Surface event forms → `xpl::model`: a plain event builds the
/// guard/deterministic special case — every `when` hoisted into one
/// pre-state filter (matching the symbolic compile), one update per `do`
/// — and its `quick` test is that guard, exact. `+=`/`-=` desugar to
/// read-modify-write, `havoc` is a range.

#include "hsc/surface/xpl_build.hh"

#include <charconv>

#include "hsc/surface/translate.hh"

namespace hsc::surface {

namespace {

[[noreturn]] void fail(const datum& d, const std::string& msg) {
  throw translate_error(d.line(), msg);
}

xpl::value as_value(const datum& d, const char* what) {
  if (!d.is_atom()) fail(d, std::string("expected an integer ") + what);
  const std::string& t = d.text();
  xpl::value v = 0;
  const auto* end = t.data() + t.size();
  const auto res = std::from_chars(t.data(), end, v);
  if (res.ec != std::errc{} || res.ptr != end) {
    fail(d,
         std::string("expected an integer ") + what + ", found '" + t + "'");
  }
  return v;
}

/// LHS ::= NAME | (at NAME EXPR) — to a target, and to the expression
/// that reads the same cell (the `+=`/`-=` desugar).
std::pair<xpl::target, lia::iexpr> read_target(const datum& d,
                                               lia::expr_factory& ex,
                                               const expr_reader& reader,
                                               const name_scope& scope) {
  xpl::target t;
  if (d.is_atom()) {
    const auto pos = scope.position(d.text());
    if (!pos) fail(d, "unknown leaf '" + d.text() + "'");
    t.cells.push_back(*pos);
    return {t, ex.variable(*pos)};
  }
  if (d.items().size() != 3 || d.head() != "at") {
    fail(d, "an assignment target is a leaf or (at ARRAY INDEX)");
  }
  const std::string& name = d.items()[1].text();
  const auto cells = scope.array(name);
  if (!cells) fail(d, "unknown array '" + name + "'");
  const lia::iexpr index = reader.read_int(d.items()[2]);
  const lia::iexpr as_read = ex.array(*cells, index);
  if (lia::expr_factory::is_const(index)) {  // static index: a plain cell
    const std::int64_t i = ex.value(index);
    if (i < 0 || i >= static_cast<std::int64_t>(cells->size())) {
      fail(d, "index " + std::to_string(i) + " out of bounds for '" + name +
                  "' (" + std::to_string(cells->size()) + " cells)");
    }
    t.cells.push_back((*cells)[static_cast<std::size_t>(i)]);
    return {t, as_read};
  }
  t.cells = *cells;
  t.index = index;
  t.indexed = true;
  return {t, as_read};
}

xpl::action read_action(const datum& d, lia::expr_factory& ex,
                        const expr_reader& reader, const name_scope& scope) {
  if (!d.is_list() || d.items().empty()) fail(d, "malformed action");
  const std::string& op = d.head();
  xpl::action a;
  if (op == "havoc") {
    if (d.items().size() != 4) fail(d, "havoc is (havoc LHS LO HI)");
    a.k = xpl::action::kind::havoc;
    a.lhs = read_target(d.items()[1], ex, reader, scope).first;
    a.lo = as_value(d.items()[2], "havoc low bound");
    a.hi = as_value(d.items()[3], "havoc high bound");
    return a;
  }
  if ((op != ":=" && op != "+=" && op != "-=") || d.items().size() != 3) {
    fail(d, "an action is (:= LHS EXPR), (+= LHS EXPR), (-= LHS EXPR) or "
            "(havoc LHS K K)");
  }
  auto [lhs, self] = read_target(d.items()[1], ex, reader, scope);
  a.lhs = std::move(lhs);
  const lia::iexpr rhs = reader.read_int(d.items()[2]);
  a.rhs = op == ":=" ? rhs : op == "+=" ? ex.add(self, rhs)
                                        : ex.sub(self, rhs);
  return a;
}

}  // namespace

xpl::model build_xpl_model(std::size_t arity, lia::expr_factory& ex,
                           const expr_reader& reader, const name_scope& scope,
                           std::span<const xpl_source> sources) {
  xpl::model m;
  m.arity = arity;
  m.ex = &ex;
  m.events.reserve(sources.size());
  for (const xpl_source& src : sources) {
    xpl::event e;
    e.name = src.name;
    e.line = src.line;
    lia::bexpr guard = lia::btrue;
    std::vector<std::uint32_t> kids;
    for (const datum& cl : src.clauses) {
      if (!cl.is_list() || cl.items().empty()) {
        fail(cl, "an event clause is (when …) or (do …)");
      }
      if (cl.head() == "when") {
        for (std::size_t i = 1; i < cl.items().size(); ++i) {
          guard = ex.conj(guard, reader.read_bool(cl.items()[i]));
        }
      } else if (cl.head() == "do") {
        xpl::term upd;
        upd.k = xpl::term::kind::update;
        for (std::size_t i = 1; i < cl.items().size(); ++i) {
          upd.acts.push_back(read_action(cl.items()[i], ex, reader, scope));
        }
        kids.push_back(m.add(std::move(upd)));
      } else {
        fail(cl, "an event clause is (when …) or (do …)");
      }
    }
    // every when hoists into one pre-state filter — the exact quick test
    e.quick = guard;
    if (guard != lia::btrue) {
      xpl::term f;
      f.k = xpl::term::kind::filter;
      f.guard = guard;
      kids.insert(kids.begin(), m.add(std::move(f)));
    }
    if (kids.size() == 1) {
      e.root = kids.front();
    } else {  // includes the empty body: a seq of nothing is the identity
      xpl::term s;
      s.k = xpl::term::kind::seq;
      s.kids = std::move(kids);
      e.root = m.add(std::move(s));
    }
    m.events.push_back(std::move(e));
  }
  m.finalize();
  return m;
}

}  // namespace hsc::surface
