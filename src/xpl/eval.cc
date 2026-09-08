/// \file eval.cc
/// \brief Concrete evaluation of `lia` expressions: int64 inside, checked
/// back into int32 at every fold step; ⊥ Kleene in guards, loud at any
/// decision. Semantics in `interpret/algorithm.md` §3.

#include "hsc/xpl/interpret/eval.hh"

#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace hsc::xpl {

namespace {

using truth = lia::expr_factory::truth;

std::string num(std::int64_t v) { return std::to_string(v); }

}  // namespace

/// Render \p e for an error message.
static std::string show(const lia::expr_factory& ex, lia::iexpr e) {
  std::ostringstream os;
  ex.print(os, e);
  return os.str();
}

/// Every fold step lands back in int32 — leaving it is a loud error.
static void check32(std::int64_t v, const lia::expr_factory& ex,
                    lia::iexpr e) {
  if (v < std::numeric_limits<value>::min() ||
      v > std::numeric_limits<value>::max()) {
    throw eval_error("overflow: " + num(v) + " in " + show(ex, e));
  }
}

std::int64_t evaluator::eval_int(lia::iexpr e, bool& undef) {
  using lia::ikind;
  if (e == lia::iundef) {
    bot("undefined expression (⊥)");
    undef = true;
    return 0;
  }
  if (lia::expr_factory::is_const(e)) return ex_.value(e);
  const lia::expr_node& n = ex_.node(e);
  const auto k = static_cast<ikind>(n.kind);
  switch (k) {
    case ikind::constant:
      return n.payload;
    case ikind::var: {
      const auto p = static_cast<std::size_t>(n.payload);
      if (p >= env_.size()) {
        throw eval_error("position " + num(static_cast<std::int64_t>(p)) +
                         " outside the state (arity " +
                         num(static_cast<std::int64_t>(env_.size())) + ")");
      }
      return env_[p];
    }
    case ikind::plus:
    case ikind::mult: {
      std::int64_t acc = k == ikind::plus ? 0 : 1;
      for (const lia::iexpr op : n.operands()) {
        const std::int64_t v = eval_int(op, undef);
        if (undef) return 0;
        acc = k == ikind::plus ? acc + v : acc * v;
        check32(acc, ex_, e);
      }
      return acc;
    }
    case ikind::array: {
      // operand 0 the index expression; operands 1… cell positions (data)
      const std::int64_t i = eval_int(n.data()[0], undef);
      if (undef) return 0;
      const std::int64_t cells = n.count - 1;
      if (i < 0 || i >= cells) {
        bot("index " + num(i) + " out of bounds (" + num(cells) +
            " cells) in " + show(ex_, e));
        undef = true;
        return 0;
      }
      const std::uint32_t p = n.data()[1 + static_cast<std::size_t>(i)];
      return env_[p];
    }
    case ikind::wrap_bool: {
      const truth t = eval_bool(n.data()[0]);
      if (t == truth::undef) {
        undef = true;
        return 0;
      }
      return t == truth::yes ? 1 : 0;
    }
    case ikind::bit_comp: {
      const std::int64_t v = eval_int(n.data()[0], undef);
      return undef ? 0 : ~static_cast<value>(v);
    }
    default:
      break;  // the binary kinds, below
  }
  const std::int64_t l = eval_int(n.data()[0], undef);
  if (undef) return 0;
  const std::int64_t r = eval_int(n.data()[1], undef);
  if (undef) return 0;
  std::int64_t v = 0;
  switch (k) {
    case ikind::minus:
      v = l - r;
      break;
    case ikind::div:
      if (r == 0) {
        bot("division by zero in " + show(ex_, e));
        undef = true;
        return 0;
      }
      v = l / r;
      break;
    case ikind::mod:
      if (r == 0) {
        bot("modulo by zero in " + show(ex_, e));
        undef = true;
        return 0;
      }
      v = l % r;
      break;
    case ikind::pow: {
      if (r < 0) {
        bot("negative exponent in " + show(ex_, e));
        undef = true;
        return 0;
      }
      v = 1;
      for (std::int64_t i = 0; i < r; ++i) {
        v *= l;
        check32(v, ex_, e);
      }
      break;
    }
    case ikind::bit_and:
      v = l & r;
      break;
    case ikind::bit_or:
      v = l | r;
      break;
    case ikind::bit_xor:
      v = l ^ r;
      break;
    case ikind::lshift:
    case ikind::rshift:
      if (r < 0 || r >= 32) {
        bot("shift by " + num(r) + " in " + show(ex_, e));
        undef = true;
        return 0;
      }
      v = k == ikind::lshift ? l << r : l >> r;
      break;
    default:
      throw eval_error("unexpected expression kind");
  }
  check32(v, ex_, e);
  return v;
}

lia::expr_factory::truth evaluator::eval_bool(lia::bexpr e) {
  using lia::bkind;
  if (e == lia::bfalse) return truth::no;
  if (e == lia::btrue) return truth::yes;
  if (e == lia::bundef) {
    bot("undefined boolean (⊥)");
    return truth::undef;
  }
  const lia::expr_node& n = ex_.bool_node(e);
  const auto k = static_cast<bkind>(n.kind);
  switch (k) {
    case bkind::conj: {
      bool saw_undef = false;
      for (const lia::bexpr op : n.operands()) {
        const truth t = eval_bool(op);
        if (t == truth::no) return truth::no;  // false absorbs ⊥ (Kleene)
        if (t == truth::undef) saw_undef = true;
      }
      return saw_undef ? truth::undef : truth::yes;
    }
    case bkind::disj: {
      bool saw_undef = false;
      for (const lia::bexpr op : n.operands()) {
        const truth t = eval_bool(op);
        if (t == truth::yes) return truth::yes;  // true absorbs ⊥ (Kleene)
        if (t == truth::undef) saw_undef = true;
      }
      return saw_undef ? truth::undef : truth::no;
    }
    case bkind::neg: {
      const truth t = eval_bool(n.data()[0]);
      if (t == truth::undef) return truth::undef;
      return t == truth::yes ? truth::no : truth::yes;
    }
    default:
      break;  // the comparisons, below
  }
  bool undef = false;
  const std::int64_t l = eval_int(n.data()[0], undef);
  if (undef) return truth::undef;
  const std::int64_t r = eval_int(n.data()[1], undef);
  if (undef) return truth::undef;
  bool v = false;
  switch (k) {
    case bkind::eq:
      v = l == r;
      break;
    case bkind::neq:
      v = l != r;
      break;
    case bkind::lt:
      v = l < r;
      break;
    case bkind::leq:
      v = l <= r;
      break;
    case bkind::gt:
      v = l > r;
      break;
    case bkind::geq:
      v = l >= r;
      break;
    default:
      throw eval_error("unexpected boolean kind");
  }
  return v ? truth::yes : truth::no;
}

lia::expr_factory::truth evaluator::guard(lia::bexpr e) {
  return eval_bool(e);
}

bool evaluator::decide(lia::bexpr e) {
  switch (eval_bool(e)) {
    case truth::yes:
      return true;
    case truth::no:
      return false;
    default:
      throw eval_error("guard is ⊥: " + cause_);
  }
}

std::int64_t evaluator::strict_int(lia::iexpr e) {
  bool undef = false;
  const std::int64_t v = eval_int(e, undef);
  if (undef) throw eval_error(cause_);
  return v;
}

}  // namespace hsc::xpl
