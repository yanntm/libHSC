/// \file surface_expr.cc
/// \brief The surface expression reader: `datum` forms → `lia` codes.
/// Grammar and coercions are the header's contract.

#include "hsc/surface/expr.hh"

#include <charconv>
#include <vector>

#include "hsc/surface/translate.hh"

namespace hsc::surface {

namespace {

[[noreturn]] void fail(const datum& d, const std::string& msg) {
  throw translate_error(d.line(), msg);
}

std::optional<std::int32_t> as_int(const datum& d) {
  if (!d.is_atom()) return std::nullopt;
  const std::string& t = d.text();
  std::int32_t v = 0;
  const auto* end = t.data() + t.size();
  const auto res = std::from_chars(t.data(), end, v);
  if (res.ec != std::errc{} || res.ptr != end) return std::nullopt;
  return v;
}

std::optional<lia::bkind> comparison(const std::string& op) {
  if (op == "==") return lia::bkind::eq;
  if (op == "!=") return lia::bkind::neq;
  if (op == "<") return lia::bkind::lt;
  if (op == "<=") return lia::bkind::leq;
  if (op == ">") return lia::bkind::gt;
  if (op == ">=") return lia::bkind::geq;
  return std::nullopt;
}

bool is_bool_head(const std::string& op) {
  return comparison(op).has_value() || op == "and" || op == "or" ||
         op == "not" || op == "imply" || op == "in";
}

}  // namespace

lia::iexpr expr_reader::read_int(const datum& d) const {
  if (d.is_atom()) {
    if (const auto v = as_int(d)) return ex_.constant(*v);
    if (const auto p = scope_.position(d.text())) return ex_.variable(*p);
    if (scope_.array(d.text())) fail(d, "array '" + d.text() + "' used bare");
    fail(d, "unknown leaf '" + d.text() + "'");
  }
  if (d.items().empty()) fail(d, "empty expression");
  const std::string& op = d.head();
  const auto& xs = d.items();
  if (is_bool_head(op)) return ex_.wrap(read_bool(d));  // 0/1 coercion
  const auto need = [&](std::size_t n) {
    if (xs.size() != n + 1) {
      fail(d, "'" + op + "' takes " + std::to_string(n) + " operands");
    }
  };
  if (op == "at") {
    need(2);
    const auto cells = scope_.array(xs[1].is_atom() ? xs[1].text() : "");
    if (!cells) fail(xs[1], "not an array");
    return ex_.array(*cells, read_int(xs[2]));
  }
  if (op == "~") {
    need(1);
    return ex_.bit_comp(read_int(xs[1]));
  }
  need(2);
  const lia::iexpr a = read_int(xs[1]);
  const lia::iexpr b = read_int(xs[2]);
  if (op == "+") return ex_.add(a, b);
  if (op == "-") return ex_.sub(a, b);
  if (op == "*") return ex_.mul(a, b);
  if (op == "/") return ex_.div(a, b);
  if (op == "%") return ex_.mod(a, b);
  if (op == "<<") return ex_.lshift(a, b);
  if (op == ">>") return ex_.rshift(a, b);
  if (op == "&") return ex_.bit_and(a, b);
  if (op == "|") return ex_.bit_or(a, b);
  if (op == "^") return ex_.bit_xor(a, b);
  fail(d, "unknown operator '" + op + "'");
}

lia::bexpr expr_reader::read_bool(const datum& d) const {
  if (d.is_list() && !d.items().empty()) {
    const std::string& op = d.head();
    const auto& xs = d.items();
    if (const auto k = comparison(op)) {
      if (xs.size() != 3) fail(d, "'" + op + "' takes two operands");
      return ex_.compare(*k, read_int(xs[1]), read_int(xs[2]));
    }
    if (op == "and" || op == "or") {
      std::vector<lia::bexpr> parts;
      for (std::size_t i = 1; i < xs.size(); ++i) {
        parts.push_back(read_bool(xs[i]));
      }
      return op == "and" ? ex_.conj(parts) : ex_.disj(parts);
    }
    if (op == "not") {
      if (xs.size() != 2) fail(d, "'not' takes one operand");
      return ex_.neg(read_bool(xs[1]));
    }
    if (op == "imply") {
      if (xs.size() != 3) fail(d, "'imply' takes two operands");
      return ex_.disj(ex_.neg(read_bool(xs[1])), read_bool(xs[2]));
    }
    if (op == "in") {  // an enumeration by nature: a disjunction of ==
      if (xs.size() < 3) fail(d, "'in' takes a value and constants");
      const lia::iexpr v = read_int(xs[1]);
      std::vector<lia::bexpr> alts;
      for (std::size_t i = 2; i < xs.size(); ++i) {
        const auto k = as_int(xs[i]);
        if (!k) fail(xs[i], "'in' expects integer constants");
        alts.push_back(ex_.compare(lia::bkind::eq, v, ex_.constant(*k)));
      }
      return ex_.disj(alts);
    }
  }
  // an integer form in boolean position reads != 0
  return ex_.compare(lia::bkind::neq, read_int(d), ex_.constant(0));
}

}  // namespace hsc::surface
