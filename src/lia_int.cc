/// \file lia_int.cc
/// \brief Integer expressions: interning, normalizing constructors,
/// substitution, support. The boolean side is `lia_bool.cc`.

#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "hsc/lia/expr.hh"
#include "hsc/util/errors.hh"

namespace hsc::lia {

namespace {

/// The probe view: a node that has not been built (the `int_set` pattern).
struct node_view {
  std::uint8_t kind;
  std::int64_t payload;
  std::span<const std::uint32_t> ops;

  [[nodiscard]] std::size_t hash() const {
    std::size_t seed = util::hash_all(
        kind, static_cast<std::uint32_t>(ops.size()), payload);
    util::hash_range(seed, ops.begin(), ops.end());
    return seed;
  }
  [[nodiscard]] bool equals(const expr_node& n) const {
    return n.kind == kind && n.count == ops.size() && n.payload == payload &&
           std::equal(ops.begin(), ops.end(), n.data());
  }
  [[nodiscard]] std::size_t extra_bytes() const {
    return ops.size() * sizeof(std::uint32_t);
  }
  expr_node* construct(void* mem) const {
    auto* p = new (mem) expr_node{
        kind, static_cast<std::uint32_t>(ops.size()), payload};
    std::copy(ops.begin(), ops.end(),
              const_cast<std::uint32_t*>(p->data()));
    return p;
  }
};

/// Inline constants: |v| < 2^30, sign-magnitude behind the low tag bit.
constexpr std::int64_t inline_limit = std::int64_t{1} << 30;

constexpr iexpr enc(std::int32_t v) {
  return v >= 0 ? (static_cast<std::uint32_t>(v) << 2) | 1u
                : (static_cast<std::uint32_t>(-v) << 2) | 3u;
}
constexpr bool is_imm(std::uint32_t e) { return (e & 1u) != 0; }
constexpr std::int32_t dec(iexpr e) {
  const auto mag = static_cast<std::int32_t>(e >> 2);
  return (e & 2u) != 0 ? -mag : mag;
}

/// A folded value must land back in int32; leaving it is loud, never silent.
std::int32_t narrow(std::int64_t v) {
  if (v < INT32_MIN || v > INT32_MAX) {
    throw overflow_error("int32 overflow folding a constant expression");
  }
  return static_cast<std::int32_t>(v);
}

}  // namespace

// --- leaves ----------------------------------------------------------------

iexpr expr_factory::constant(std::int32_t v) {
  if (v > -inline_limit && v < inline_limit) return enc(v);
  return intern(static_cast<std::uint8_t>(ikind::constant), v, {});
}

iexpr expr_factory::variable(std::uint32_t pos) {
  return intern(static_cast<std::uint8_t>(ikind::var), pos, {});
}

std::int32_t expr_factory::value(iexpr e) const {
  if (is_imm(e)) return dec(e);
  return static_cast<std::int32_t>(node(e).payload);
}

ikind expr_factory::kind(iexpr e) const {
  return static_cast<ikind>(node(e).kind);
}

const expr_node& expr_factory::node(iexpr e) const { return inodes_[e >> 1]; }
const expr_node& expr_factory::bool_node(bexpr e) const {
  return bnodes_[e >> 1];
}
bkind expr_factory::bool_kind(bexpr e) const {
  return static_cast<bkind>(bool_node(e).kind);
}

iexpr expr_factory::intern(std::uint8_t k, std::int64_t payload,
                           std::span<const std::uint32_t> ops) {
  return inodes_.get(node_view{k, payload, ops}) << 1;
}
bexpr expr_factory::intern_bool(std::uint8_t k, std::int64_t payload,
                                std::span<const std::uint32_t> ops) {
  return bnodes_.get(node_view{k, payload, ops}) << 1;
}

/// A value-bearing code: an inline constant or a wide constant node.
static bool is_value(const expr_factory& f, iexpr e) {
  if (is_imm(e)) return e != iundef;
  return f.kind(e) == ikind::constant;
}

// --- n-ary plus / mult -----------------------------------------------------

iexpr expr_factory::nary(ikind k, std::span<const iexpr> xs) {
  const bool is_plus = k == ikind::plus;
  std::int64_t acc = is_plus ? 0 : 1;
  std::vector<iexpr> flat;
  flat.reserve(xs.size());

  // Flatten same-kind children; merge every constant through the neutral
  // element; ⊥ poisons.
  for (const iexpr x : xs) {
    if (x == iundef) return iundef;
    if (is_value(*this, x)) {
      const std::int64_t v = value(x);
      if (is_plus) {
        acc += v;  // int64 cannot overflow summing int32 terms
      } else {
        if (__builtin_mul_overflow(acc, v, &acc)) {
          throw overflow_error("int32 overflow folding a product");
        }
      }
    } else if (!is_imm(x) && kind(x) == k) {
      for (const iexpr op : node(x).operands()) {
        if (is_value(*this, op)) {
          if (is_plus) {
            acc += value(op);
          } else if (__builtin_mul_overflow(acc, value(op), &acc)) {
            throw overflow_error("int32 overflow folding a product");
          }
        } else {
          flat.push_back(op);
        }
      }
    } else {
      flat.push_back(x);
    }
  }

  if (!is_plus && acc == 0) return constant(0);  // absorbing
  const std::int32_t c = narrow(acc);
  const bool neutral = c == (is_plus ? 0 : 1);
  if (!neutral || flat.empty()) flat.push_back(constant(c));
  if (flat.size() == 1) return flat.front();
  std::ranges::sort(flat);  // canonical operand order, duplicates kept
  return intern(static_cast<std::uint8_t>(k), 0, flat);
}

iexpr expr_factory::add(std::span<const iexpr> xs) {
  return nary(ikind::plus, xs);
}
iexpr expr_factory::add(iexpr a, iexpr b) {
  const iexpr ops[] = {a, b};
  return nary(ikind::plus, ops);
}
iexpr expr_factory::mul(std::span<const iexpr> xs) {
  return nary(ikind::mult, xs);
}
iexpr expr_factory::mul(iexpr a, iexpr b) {
  const iexpr ops[] = {a, b};
  return nary(ikind::mult, ops);
}

// --- binary operators ------------------------------------------------------

std::int64_t expr_factory::fold(ikind k, std::int64_t l, std::int64_t r,
                                bool& undef) {
  switch (k) {
    case ikind::minus: return l - r;
    case ikind::div:
      if (r == 0) { undef = true; return 0; }
      return l / r;
    case ikind::mod:
      if (r == 0) { undef = true; return 0; }
      return l % r;
    case ikind::pow: {
      if (r < 0) { undef = true; return 0; }
      std::int64_t acc = 1;
      for (std::int64_t i = 0; i < r; ++i) {
        if (__builtin_mul_overflow(acc, l, &acc)) {
          throw overflow_error("int32 overflow folding a power");
        }
      }
      return acc;
    }
    case ikind::bit_and: return l & r;
    case ikind::bit_or: return l | r;
    case ikind::bit_xor: return l ^ r;
    case ikind::lshift: {
      if (r < 0 || r >= 32) { undef = true; return 0; }
      std::int64_t out = 0;
      if (__builtin_mul_overflow(l, std::int64_t{1} << r, &out)) {
        throw overflow_error("overflow folding a left shift");
      }
      return out;
    }
    case ikind::rshift:
      if (r < 0 || r >= 32) { undef = true; return 0; }
      return l >> r;
    default: return 0;  // unreachable: nary and leaves never fold here
  }
}

iexpr expr_factory::binary(ikind k, iexpr a, iexpr b) {
  if (a == iundef || b == iundef) return iundef;
  if (is_value(*this, a) && is_value(*this, b)) {
    bool undef = false;
    const std::int64_t v = fold(k, value(a), value(b), undef);
    return undef ? iundef : constant(narrow(v));
  }
  const std::uint32_t ops[] = {a, b};
  return intern(static_cast<std::uint8_t>(k), 0, ops);
}

iexpr expr_factory::sub(iexpr a, iexpr b) { return binary(ikind::minus, a, b); }
iexpr expr_factory::div(iexpr a, iexpr b) { return binary(ikind::div, a, b); }
iexpr expr_factory::mod(iexpr a, iexpr b) { return binary(ikind::mod, a, b); }
iexpr expr_factory::pow(iexpr a, iexpr b) { return binary(ikind::pow, a, b); }
iexpr expr_factory::bit_and(iexpr a, iexpr b) {
  return binary(ikind::bit_and, a, b);
}
iexpr expr_factory::bit_or(iexpr a, iexpr b) {
  return binary(ikind::bit_or, a, b);
}
iexpr expr_factory::bit_xor(iexpr a, iexpr b) {
  return binary(ikind::bit_xor, a, b);
}
iexpr expr_factory::lshift(iexpr a, iexpr b) {
  return binary(ikind::lshift, a, b);
}
iexpr expr_factory::rshift(iexpr a, iexpr b) {
  return binary(ikind::rshift, a, b);
}

iexpr expr_factory::bit_comp(iexpr a) {
  if (a == iundef) return iundef;
  if (is_value(*this, a)) return constant(~value(a));
  const std::uint32_t ops[] = {a};
  return intern(static_cast<std::uint8_t>(ikind::bit_comp), 0, ops);
}

// --- arrays and bool wrapping ----------------------------------------------

iexpr expr_factory::array(std::span<const std::uint32_t> cells, iexpr index) {
  if (index == iundef) return iundef;
  if (is_value(*this, index)) {
    const std::int32_t i = value(index);
    if (i < 0 || static_cast<std::size_t>(i) >= cells.size()) {
      return iundef;  // out of bounds is ⊥
    }
    return variable(cells[static_cast<std::size_t>(i)]);
  }
  // Operand 0 the index expression, then the placement — raw positions,
  // not expression codes; every walker treats them as data.
  std::vector<std::uint32_t> ops;
  ops.reserve(cells.size() + 1);
  ops.push_back(index);
  ops.insert(ops.end(), cells.begin(), cells.end());
  return intern(static_cast<std::uint8_t>(ikind::array), 0, ops);
}

iexpr expr_factory::wrap(bexpr b) {
  if (b == btrue) return constant(1);
  if (b == bfalse) return constant(0);
  if (b == bundef) return iundef;
  const std::uint32_t ops[] = {b};
  return intern(static_cast<std::uint8_t>(ikind::wrap_bool), 0, ops);
}

// --- substitution: the currying step ---------------------------------------

iexpr expr_factory::subst(iexpr e, std::uint32_t pos, iexpr v) {
  if (is_imm(e)) return e;
  const expr_node& n = node(e);
  switch (static_cast<ikind>(n.kind)) {
    case ikind::constant: return e;
    case ikind::var:
      return n.payload == pos ? v : e;
    case ikind::plus:
    case ikind::mult: {
      std::vector<iexpr> ops(n.operands().begin(), n.operands().end());
      for (iexpr& op : ops) op = subst(op, pos, v);
      return nary(static_cast<ikind>(n.kind), ops);
    }
    case ikind::minus: case ikind::div: case ikind::mod: case ikind::pow:
    case ikind::bit_and: case ikind::bit_or: case ikind::bit_xor:
    case ikind::lshift: case ikind::rshift:
      return binary(static_cast<ikind>(n.kind),
                    subst(n.operands()[0], pos, v),
                    subst(n.operands()[1], pos, v));
    case ikind::bit_comp:
      return bit_comp(subst(n.operands()[0], pos, v));
    case ikind::array:
      // The cells are placement data, never substituted into; only the
      // index resolves, and its grounding folds the access to a variable.
      return array(n.operands().subspan(1),
                   subst(n.operands()[0], pos, v));
    case ikind::wrap_bool:
      return wrap(subst_bool(n.operands()[0], pos, v));
  }
  return e;
}

iexpr expr_factory::subst_subexpr(iexpr e, iexpr sub, iexpr v) {
  if (e == sub) return v;
  if (is_imm(e)) return e;
  const expr_node& n = node(e);
  switch (static_cast<ikind>(n.kind)) {
    case ikind::constant:
    case ikind::var:
      return e;
    case ikind::plus:
    case ikind::mult: {
      std::vector<iexpr> ops(n.operands().begin(), n.operands().end());
      for (iexpr& op : ops) op = subst_subexpr(op, sub, v);
      return nary(static_cast<ikind>(n.kind), ops);
    }
    case ikind::minus: case ikind::div: case ikind::mod: case ikind::pow:
    case ikind::bit_and: case ikind::bit_or: case ikind::bit_xor:
    case ikind::lshift: case ikind::rshift:
      return binary(static_cast<ikind>(n.kind),
                    subst_subexpr(n.operands()[0], sub, v),
                    subst_subexpr(n.operands()[1], sub, v));
    case ikind::bit_comp:
      return bit_comp(subst_subexpr(n.operands()[0], sub, v));
    case ikind::array:
      // Cells are placement data; only the index is an expression.
      return array(n.operands().subspan(1),
                   subst_subexpr(n.operands()[0], sub, v));
    case ikind::wrap_bool:
      return wrap(subst_subexpr_bool(n.operands()[0], sub, v));
  }
  return e;
}

// --- evaluation ------------------------------------------------------------

std::int64_t expr_factory::eval_int(iexpr e, std::span<const std::int32_t> env,
                                    bool& undef) const {
  if (is_imm(e)) {
    if (e == iundef) { undef = true; return 0; }
    return dec(e);
  }
  const expr_node& n = node(e);
  const auto k = static_cast<ikind>(n.kind);
  switch (k) {
    case ikind::constant: return n.payload;
    case ikind::var: {
      const auto pos = static_cast<std::size_t>(n.payload);
      if (pos >= env.size()) {
        throw std::logic_error("lia eval: position outside the environment");
      }
      return env[pos];
    }
    case ikind::plus:
    case ikind::mult: {
      std::int64_t acc = k == ikind::plus ? 0 : 1;
      for (const iexpr op : n.operands()) {
        const std::int64_t v = eval_int(op, env, undef);
        if (undef) return 0;
        const bool over = k == ikind::plus
                              ? __builtin_add_overflow(acc, v, &acc)
                              : __builtin_mul_overflow(acc, v, &acc);
        if (over) throw overflow_error("overflow evaluating an expression");
      }
      return acc;
    }
    case ikind::bit_comp: {
      const std::int64_t v = eval_int(n.operands()[0], env, undef);
      return undef ? 0 : ~v;
    }
    case ikind::array:
      undef = true;  // an unresolved index cannot be read through an env
      return 0;
    case ikind::wrap_bool:
      switch (eval_bool(n.operands()[0], env)) {
        case truth::yes: return 1;
        case truth::no: return 0;
        case truth::undef: undef = true; return 0;
      }
      return 0;
    default: {  // the binary operators share fold()
      const std::int64_t l = eval_int(n.operands()[0], env, undef);
      if (undef) return 0;
      const std::int64_t r = eval_int(n.operands()[1], env, undef);
      if (undef) return 0;
      return fold(k, l, r, undef);
    }
  }
}

// --- reading ---------------------------------------------------------------

namespace {
void collect(const expr_factory& f, iexpr e, std::vector<std::uint32_t>& out) {
  if (is_imm(e)) return;
  const expr_node& n = f.node(e);
  const auto k = static_cast<ikind>(n.kind);
  if (k == ikind::constant) return;
  if (k == ikind::var) {
    out.push_back(static_cast<std::uint32_t>(n.payload));
    return;
  }
  if (k == ikind::wrap_bool) {
    for (const std::uint32_t p : f.support_bool(n.operands()[0])) {
      out.push_back(p);
    }
    return;
  }
  if (k == ikind::array) {  // the cells are placement data, not reads
    collect(f, n.operands()[0], out);
    return;
  }
  for (const iexpr op : n.operands()) collect(f, op, out);
}
}  // namespace

std::vector<std::uint32_t> expr_factory::support(iexpr e) const {
  std::vector<std::uint32_t> out;
  collect(*this, e, out);
  std::ranges::sort(out);
  const auto dup = std::ranges::unique(out);
  out.erase(dup.begin(), dup.end());
  return out;
}

namespace {
void collect_arrays(const expr_factory& f, iexpr e,
                    std::vector<std::uint32_t>& out) {
  if (is_imm(e)) return;
  const expr_node& n = f.node(e);
  switch (static_cast<ikind>(n.kind)) {
    case ikind::constant:
    case ikind::var:
      return;
    case ikind::array: {
      const auto ops = n.operands();
      out.insert(out.end(), ops.begin() + 1, ops.end());
      collect_arrays(f, ops[0], out);
      return;
    }
    case ikind::wrap_bool:
      for (const std::uint32_t p : f.array_positions_bool(n.operands()[0])) {
        out.push_back(p);
      }
      return;
    default:
      for (const iexpr op : n.operands()) collect_arrays(f, op, out);
      return;
  }
}
}  // namespace

std::vector<std::uint32_t> expr_factory::array_positions(iexpr e) const {
  std::vector<std::uint32_t> out;
  collect_arrays(*this, e, out);
  std::ranges::sort(out);
  const auto dup = std::ranges::unique(out);
  out.erase(dup.begin(), dup.end());
  return out;
}

iexpr expr_factory::shift_positions(iexpr e, std::int32_t delta) {
  if (delta == 0 || is_imm(e)) return e;
  const expr_node& n = node(e);
  switch (static_cast<ikind>(n.kind)) {
    case ikind::constant:
      return e;
    case ikind::var:
      return variable(static_cast<std::uint32_t>(
          static_cast<std::int64_t>(n.payload) + delta));
    case ikind::array: {
      const auto ops = n.operands();
      std::vector<std::uint32_t> cells(ops.begin() + 1, ops.end());
      for (std::uint32_t& c : cells) {
        c = static_cast<std::uint32_t>(static_cast<std::int64_t>(c) + delta);
      }
      return array(cells, shift_positions(ops[0], delta));
    }
    case ikind::plus:
    case ikind::mult: {
      std::vector<iexpr> ops(n.operands().begin(), n.operands().end());
      for (iexpr& op : ops) op = shift_positions(op, delta);
      return nary(static_cast<ikind>(n.kind), ops);
    }
    case ikind::bit_comp:
      return bit_comp(shift_positions(n.operands()[0], delta));
    case ikind::wrap_bool:
      return wrap(shift_positions_bool(n.operands()[0], delta));
    default:
      return binary(static_cast<ikind>(n.kind),
                    shift_positions(n.operands()[0], delta),
                    shift_positions(n.operands()[1], delta));
  }
}

iexpr expr_factory::first_subexpr(iexpr e) const {
  if (is_imm(e)) return e;
  const expr_node& n = node(e);
  const auto k = static_cast<ikind>(n.kind);
  if (k == ikind::constant || k == ikind::var) return e;
  if (k == ikind::array) return n.operands()[0];  // peel one layer per call
  for (const iexpr op : n.operands()) {
    const iexpr r = first_subexpr(op);
    if (r != op) return r;
  }
  return e;
}

void expr_factory::print(std::ostream& os, iexpr e) const {
  if (e == iundef) { os << "undef"; return; }
  if (is_imm(e)) { os << dec(e); return; }
  const expr_node& n = node(e);
  const auto k = static_cast<ikind>(n.kind);
  const auto binop = [&](const char* op) {
    os << '(';
    print(os, n.operands()[0]);
    os << ' ' << op << ' ';
    print(os, n.operands()[1]);
    os << ')';
  };
  switch (k) {
    case ikind::constant: os << n.payload; return;
    case ikind::var: os << 'v' << n.payload; return;
    case ikind::plus:
    case ikind::mult: {
      const char* op = k == ikind::plus ? " + " : " * ";
      os << '(';
      bool first = true;
      for (const iexpr x : n.operands()) {
        if (!first) os << op;
        first = false;
        print(os, x);
      }
      os << ')';
      return;
    }
    case ikind::minus: binop("-"); return;
    case ikind::div: binop("/"); return;
    case ikind::mod: binop("%"); return;
    case ikind::pow: binop("**"); return;
    case ikind::bit_and: binop("&"); return;
    case ikind::bit_or: binop("|"); return;
    case ikind::bit_xor: binop("^"); return;
    case ikind::lshift: binop("<<"); return;
    case ikind::rshift: binop(">>"); return;
    case ikind::bit_comp:
      os << "~";
      print(os, n.operands()[0]);
      return;
    case ikind::array: {
      // placement then index: t{p0,p1,…}[index]
      os << "t{";
      const auto ops = n.operands();
      for (std::size_t i = 1; i < ops.size(); ++i) {
        if (i > 1) os << ',';
        os << ops[i];
      }
      os << "}[";
      print(os, ops[0]);
      os << ']';
      return;
    }
    case ikind::wrap_bool:
      os << "int(";
      print_bool(os, n.operands()[0]);
      os << ')';
      return;
  }
}

}  // namespace hsc::lia
