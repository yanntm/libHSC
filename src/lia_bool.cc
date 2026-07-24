/// \file lia_bool.cc
/// \brief Boolean expressions: comparisons, and/or/not, negation normal
/// form, substitution. The integer side is `lia_int.cc`.

#include <algorithm>
#include <ostream>
#include <vector>

#include "hsc/lia/expr.hh"

namespace hsc::lia {

namespace {

constexpr bool is_imm(std::uint32_t e) { return (e & 1u) != 0; }

/// A comparison folded over two ground values.
constexpr bool cmp_eval(bkind k, std::int64_t l, std::int64_t r) {
  switch (k) {
    case bkind::eq: return l == r;
    case bkind::neq: return l != r;
    case bkind::lt: return l < r;
    case bkind::leq: return l <= r;
    case bkind::gt: return l > r;
    case bkind::geq: return l >= r;
    default: return false;  // unreachable: not a comparison
  }
}

/// The comparison `not (l k r)` is, without moving the operands.
constexpr bkind flipped(bkind k) {
  switch (k) {
    case bkind::eq: return bkind::neq;
    case bkind::neq: return bkind::eq;
    case bkind::lt: return bkind::geq;
    case bkind::geq: return bkind::lt;
    case bkind::gt: return bkind::leq;
    case bkind::leq: return bkind::gt;
    default: return k;  // unreachable: not a comparison
  }
}

constexpr bool is_comparison(bkind k) {
  return k != bkind::conj && k != bkind::disj && k != bkind::neg;
}

}  // namespace

// --- comparisons -----------------------------------------------------------

bexpr expr_factory::compare(bkind op, iexpr l, iexpr r) {
  if (l == iundef || r == iundef) return bundef;
  const bool lv = is_const(l) || (!is_imm(l) && kind(l) == ikind::constant);
  const bool rv = is_const(r) || (!is_imm(r) && kind(r) == ikind::constant);
  if (lv && rv) return cmp_eval(op, value(l), value(r)) ? btrue : bfalse;
  if (l == r) {
    // x op x on any expression: reflexivity decides.
    switch (op) {
      case bkind::eq: case bkind::leq: case bkind::geq: return btrue;
      case bkind::neq: case bkind::lt: case bkind::gt: return bfalse;
      default: break;
    }
  }
  if ((op == bkind::eq || op == bkind::neq) && l > r) {
    std::swap(l, r);  // commutative: one code for both spellings
  }
  const std::uint32_t ops[] = {l, r};
  return intern_bool(static_cast<std::uint8_t>(op), 0, ops);
}

// --- n-ary and / or --------------------------------------------------------

bexpr expr_factory::nary_bool(bkind k, std::span<const bexpr> xs) {
  const bool is_and = k == bkind::conj;
  const bexpr neutral = is_and ? btrue : bfalse;
  const bexpr absorbing = is_and ? bfalse : btrue;
  std::vector<bexpr> flat;
  flat.reserve(xs.size());

  // Strong Kleene: a decided absorbing constant wins even beside ⊥
  // (false && ⊥ is false, true || ⊥ is true). Commutative and De
  // Morgan-sound, which the normal form requires; at a guard,
  // observationally the lazy &&/|| the models were written for, since ⊥
  // and false both refuse (abort is the algebra's 0). An ⊥ operand beside
  // *undecided* operands is kept in the node — the state decides: a false
  // conjunct still absorbs it, only a reached ⊥ poisons (see eval_bool).
  for (const bexpr x : xs) {
    if (x == absorbing) return absorbing;
  }
  for (const bexpr x : xs) {
    if (x == neutral) continue;
    if (!is_imm(x) && bool_kind(x) == k) {
      for (const bexpr op : bool_node(x).operands()) flat.push_back(op);
    } else {
      flat.push_back(x);  // includes a bare ⊥: kept, adjudicated per state
    }
  }
  if (flat.empty()) return neutral;
  std::ranges::sort(flat);  // canonical order; idempotent, so dedup
  const auto dup = std::ranges::unique(flat);
  flat.erase(dup.begin(), dup.end());
  if (flat.size() == 1) return flat.front();
  return intern_bool(static_cast<std::uint8_t>(k), 0, flat);
}

bexpr expr_factory::conj(std::span<const bexpr> xs) {
  return nary_bool(bkind::conj, xs);
}
bexpr expr_factory::conj(bexpr a, bexpr b) {
  const bexpr ops[] = {a, b};
  return nary_bool(bkind::conj, ops);
}
bexpr expr_factory::disj(std::span<const bexpr> xs) {
  return nary_bool(bkind::disj, xs);
}
bexpr expr_factory::disj(bexpr a, bexpr b) {
  const bexpr ops[] = {a, b};
  return nary_bool(bkind::disj, ops);
}

// --- negation --------------------------------------------------------------

bexpr expr_factory::neg(bexpr a) {
  if (a == btrue) return bfalse;
  if (a == bfalse) return btrue;
  if (a == bundef) return bundef;
  const expr_node& n = bool_node(a);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    return compare(flipped(k), n.operands()[0], n.operands()[1]);
  }
  if (k == bkind::neg) return n.operands()[0];  // not not e
  const std::uint32_t ops[] = {a};
  return intern_bool(static_cast<std::uint8_t>(bkind::neg), 0, ops);
}

bexpr expr_factory::push_negations(bexpr a) {
  if (is_imm(a)) return a;
  const expr_node& n = bool_node(a);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) return a;
  std::vector<bexpr> ops(n.operands().begin(), n.operands().end());
  if (k == bkind::neg) {
    // De Morgan through the child, then keep pushing.
    const expr_node& c = bool_node(ops[0]);
    const auto ck = static_cast<bkind>(c.kind);
    std::vector<bexpr> inner(c.operands().begin(), c.operands().end());
    for (bexpr& op : inner) op = push_negations(neg(op));
    return ck == bkind::conj ? disj(inner) : conj(inner);
  }
  for (bexpr& op : ops) op = push_negations(op);
  return nary_bool(k, ops);
}

// --- substitution ----------------------------------------------------------

bexpr expr_factory::subst_bool(bexpr e, std::uint32_t pos, iexpr v) {
  if (is_imm(e)) return e;
  const expr_node& n = bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    return compare(k, subst(n.operands()[0], pos, v),
                   subst(n.operands()[1], pos, v));
  }
  if (k == bkind::neg) return neg(subst_bool(n.operands()[0], pos, v));
  std::vector<bexpr> ops(n.operands().begin(), n.operands().end());
  for (bexpr& op : ops) op = subst_bool(op, pos, v);
  return nary_bool(k, ops);
}

bexpr expr_factory::subst_subexpr_bool(bexpr e, iexpr sub, iexpr v) {
  if (is_imm(e)) return e;
  const expr_node& n = bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    return compare(k, subst_subexpr(n.operands()[0], sub, v),
                   subst_subexpr(n.operands()[1], sub, v));
  }
  if (k == bkind::neg) return neg(subst_subexpr_bool(n.operands()[0], sub, v));
  std::vector<bexpr> ops(n.operands().begin(), n.operands().end());
  for (bexpr& op : ops) op = subst_subexpr_bool(op, sub, v);
  return nary_bool(k, ops);
}

// --- evaluation ------------------------------------------------------------

expr_factory::truth expr_factory::eval_bool(
    bexpr e, std::span<const std::int32_t> env) const {
  if (e == btrue) return truth::yes;
  if (e == bfalse) return truth::no;
  if (e == bundef) return truth::undef;
  const expr_node& n = bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    bool undef = false;
    const std::int64_t l = eval_int(n.operands()[0], env, undef);
    if (undef) return truth::undef;
    const std::int64_t r = eval_int(n.operands()[1], env, undef);
    if (undef) return truth::undef;
    return cmp_eval(k, l, r) ? truth::yes : truth::no;
  }
  if (k == bkind::neg) {
    switch (eval_bool(n.operands()[0], env)) {
      case truth::yes: return truth::no;
      case truth::no: return truth::yes;
      case truth::undef: return truth::undef;
    }
  }
  // conj / disj, strong Kleene (order-free lazy &&/||): a decided
  // absorbing value wins even beside ⊥; an ⊥ reached in an undecided node
  // poisons it.
  const bool is_and = k == bkind::conj;
  bool absorbed = false;
  bool undef = false;
  for (const bexpr op : n.operands()) {
    switch (eval_bool(op, env)) {
      case truth::undef: undef = true; break;
      case truth::yes:
        if (!is_and) absorbed = true;
        break;
      case truth::no:
        if (is_and) absorbed = true;
        break;
    }
  }
  if (absorbed) return is_and ? truth::no : truth::yes;
  if (undef) return truth::undef;
  return is_and ? truth::yes : truth::no;
}

// --- reading ---------------------------------------------------------------

std::vector<std::uint32_t> expr_factory::support_bool(bexpr e) const {
  std::vector<std::uint32_t> out;
  if (is_imm(e)) return out;
  const expr_node& n = bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    for (const iexpr op : n.operands()) {
      for (const std::uint32_t p : support(op)) out.push_back(p);
    }
  } else {
    for (const bexpr op : n.operands()) {
      for (const std::uint32_t p : support_bool(op)) out.push_back(p);
    }
  }
  std::ranges::sort(out);
  const auto dup = std::ranges::unique(out);
  out.erase(dup.begin(), dup.end());
  return out;
}

std::vector<std::uint32_t> expr_factory::array_positions_bool(bexpr e) const {
  std::vector<std::uint32_t> out;
  if (is_imm(e)) return out;
  const expr_node& n = bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    for (const iexpr op : n.operands()) {
      for (const std::uint32_t p : array_positions(op)) out.push_back(p);
    }
  } else {
    for (const bexpr op : n.operands()) {
      for (const std::uint32_t p : array_positions_bool(op)) out.push_back(p);
    }
  }
  std::ranges::sort(out);
  const auto dup = std::ranges::unique(out);
  out.erase(dup.begin(), dup.end());
  return out;
}

bexpr expr_factory::shift_positions_bool(bexpr e, std::int32_t delta) {
  if (delta == 0 || is_imm(e)) return e;
  const expr_node& n = bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    return compare(k, shift_positions(n.operands()[0], delta),
                   shift_positions(n.operands()[1], delta));
  }
  if (k == bkind::neg) return neg(shift_positions_bool(n.operands()[0], delta));
  std::vector<bexpr> ops(n.operands().begin(), n.operands().end());
  for (bexpr& op : ops) op = shift_positions_bool(op, delta);
  return nary_bool(k, ops);
}

void expr_factory::print_bool(std::ostream& os, bexpr e) const {
  if (e == btrue) { os << "true"; return; }
  if (e == bfalse) { os << "false"; return; }
  if (e == bundef) { os << "undef"; return; }
  const expr_node& n = bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  if (is_comparison(k)) {
    const char* op = k == bkind::eq    ? "=="
                     : k == bkind::neq ? "!="
                     : k == bkind::lt  ? "<"
                     : k == bkind::leq ? "<="
                     : k == bkind::gt  ? ">"
                                       : ">=";
    os << '(';
    print(os, n.operands()[0]);
    os << ' ' << op << ' ';
    print(os, n.operands()[1]);
    os << ')';
    return;
  }
  if (k == bkind::neg) {
    os << '!';
    print_bool(os, n.operands()[0]);
    return;
  }
  const char* op = k == bkind::conj ? " && " : " || ";
  os << '(';
  bool first = true;
  for (const bexpr x : n.operands()) {
    if (!first) os << op;
    first = false;
    print_bool(os, x);
  }
  os << ')';
}

}  // namespace hsc::lia
