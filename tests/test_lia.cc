/// \file test_lia.cc
/// \brief The lia expression package: canonicity by interning, the factory
/// normal form, ⊥, and substitution as currying.

#include <doctest/doctest.h>

#include <sstream>

#include "hsc/lia/expr.hh"
#include "hsc/util/errors.hh"

using namespace hsc::lia;

namespace {
std::string show(const expr_factory& f, iexpr e) {
  std::ostringstream os;
  f.print(os, e);
  return os.str();
}
std::string show_bool(const expr_factory& f, bexpr e) {
  std::ostringstream os;
  f.print_bool(os, e);
  return os.str();
}
}  // namespace

TEST_CASE("constants live in the code, not the table") {
  expr_factory f;
  CHECK(f.constant(0) == f.constant(0));
  CHECK(f.constant(5) != f.constant(-5));
  CHECK(f.value(f.constant(5)) == 5);
  CHECK(f.value(f.constant(-5)) == -5);
  CHECK(f.value(f.constant(0)) == 0);
  CHECK(expr_factory::is_const(f.constant(-7)));
  CHECK(f.constant(3) != iundef);  // ⊥ is the spare "-0", not a constant

  // beyond 30-bit magnitude: a wide node, same value read back
  const std::int32_t big = 1 << 30;
  CHECK_FALSE(expr_factory::is_const(f.constant(big)));
  CHECK(f.value(f.constant(big)) == big);
  CHECK(f.value(f.constant(-big)) == -big);
  CHECK(f.constant(big) == f.constant(big));  // interned once
}

TEST_CASE("the normal form: fold, flatten, sort, collapse") {
  expr_factory f;
  const iexpr x = f.variable(0);
  const iexpr y = f.variable(1);

  // x + y == y + x, one code
  CHECK(f.add(x, y) == f.add(y, x));
  // (x + 1) + 2 folds through: x + 3
  CHECK(f.add(f.add(x, f.constant(1)), f.constant(2)) ==
        f.add(x, f.constant(3)));
  // neutral vanishes, singleton collapses
  CHECK(f.add(x, f.constant(0)) == x);
  CHECK(f.mul(x, f.constant(1)) == x);
  // absorbing zero
  CHECK(f.mul(x, f.constant(0)) == f.constant(0));
  // duplicates kept for + and *
  CHECK(f.add(x, x) != x);
  // ground arithmetic folds to a constant
  CHECK(f.sub(f.constant(7), f.constant(9)) == f.constant(-2));
  CHECK(f.mod(f.constant(7), f.constant(4)) == f.constant(3));
  CHECK(f.pow(f.constant(2), f.constant(10)) == f.constant(1024));
}

TEST_CASE("partial operations land on ⊥ and ⊥ poisons") {
  expr_factory f;
  const iexpr x = f.variable(0);
  CHECK(f.div(x, f.constant(0)) != iundef);  // not ground: stays symbolic
  CHECK(f.div(f.constant(1), f.constant(0)) == iundef);
  CHECK(f.mod(f.constant(1), f.constant(0)) == iundef);
  CHECK(f.pow(f.constant(2), f.constant(-1)) == iundef);
  CHECK(f.lshift(f.constant(1), f.constant(40)) == iundef);
  CHECK(f.add(x, iundef) == iundef);
  CHECK(f.mul(iundef, x) == iundef);
  CHECK(f.compare(bkind::lt, iundef, x) == bundef);
  CHECK(f.conj(f.compare(bkind::lt, x, f.constant(3)), bundef) == bundef);
  CHECK(f.conj(bfalse, bundef) == bundef);  // poison beats absorption
}

TEST_CASE("overflow in a fold is loud") {
  expr_factory f;
  const std::int32_t big = INT32_MAX;
  CHECK_THROWS_AS((void)f.add(f.constant(big), f.constant(1)),
                  hsc::overflow_error);
  CHECK_THROWS_AS((void)f.mul(f.constant(big), f.constant(2)),
                  hsc::overflow_error);
}

TEST_CASE("comparisons: fold, reflexivity, orientation of ==") {
  expr_factory f;
  const iexpr x = f.variable(0);
  const iexpr y = f.variable(1);
  CHECK(f.compare(bkind::lt, f.constant(2), f.constant(3)) == btrue);
  CHECK(f.compare(bkind::geq, f.constant(2), f.constant(3)) == bfalse);
  // x op x needs no valuation
  CHECK(f.compare(bkind::leq, x, x) == btrue);
  CHECK(f.compare(bkind::lt, x, x) == bfalse);
  // == and != are commutative: one code for both spellings
  CHECK(f.compare(bkind::eq, x, y) == f.compare(bkind::eq, y, x));
  // ordered comparisons stay as written
  CHECK(f.compare(bkind::lt, x, y) != f.compare(bkind::gt, y, x));
}

TEST_CASE("and/or: flatten, absorb, dedup; negation flips comparisons") {
  expr_factory f;
  const iexpr x = f.variable(0);
  const bexpr a = f.compare(bkind::lt, x, f.constant(3));
  const bexpr b = f.compare(bkind::geq, x, f.constant(1));
  CHECK(f.conj(a, btrue) == a);
  CHECK(f.conj(a, bfalse) == bfalse);
  CHECK(f.disj(a, btrue) == btrue);
  CHECK(f.conj(a, a) == a);  // idempotent
  CHECK(f.conj(f.conj(a, b), a) == f.conj(a, b));  // flatten + dedup
  // !(x < 3) is x >= 3, no not-node
  CHECK(f.neg(a) == f.compare(bkind::geq, x, f.constant(3)));
  CHECK(f.neg(f.neg(f.conj(a, b))) == f.conj(a, b));
  // De Morgan on demand
  CHECK(f.push_negations(f.neg(f.conj(a, b))) ==
        f.disj(f.neg(a), f.neg(b)));
}

TEST_CASE("substitution is currying: residuals then ground") {
  expr_factory f;
  const iexpr x = f.variable(0);
  const iexpr y = f.variable(1);

  // x + y with x := 3 leaves the residual y + 3
  CHECK(f.subst(f.add(x, y), 0, f.constant(3)) == f.add(y, f.constant(3)));
  // x < y with x := 3: residual 3 < y; then y := 5 grounds to true
  const bexpr r = f.subst_bool(f.compare(bkind::lt, x, y), 0, f.constant(3));
  CHECK(r == f.compare(bkind::lt, f.constant(3), y));
  CHECK(f.subst_bool(r, 1, f.constant(5)) == btrue);
  CHECK(f.subst_bool(r, 1, f.constant(2)) == bfalse);
  // substituting an absent position is identity
  CHECK(f.subst(f.add(x, y), 7, f.constant(1)) == f.add(x, y));
  // variables substitute for variables (reindexing)
  CHECK(f.subst(f.add(x, y), 0, f.variable(2)) ==
        f.add(f.variable(2), y));
}

TEST_CASE("arrays: placement in the node, folding, spread cells") {
  expr_factory f;
  const iexpr x = f.variable(0);
  // tab's four cells sit at positions 5, 2, 9, 7 — spread and permuted;
  // nothing assumes adjacency.
  const std::uint32_t tab[] = {5, 2, 9, 7};

  CHECK(f.array(tab, f.constant(7)) == iundef);        // out of bounds
  CHECK(f.array(tab, f.constant(2)) == f.variable(9)); // folds to the cell
  const iexpr a = f.array(tab, x);                     // index unresolved
  CHECK(f.kind(a) == ikind::array);
  CHECK(f.support(a) == std::vector<std::uint32_t>{0});  // cells are data
  CHECK(f.array_positions(a) == std::vector<std::uint32_t>{2, 5, 7, 9});
  // currying the index folds the access to the cell's variable
  CHECK(f.subst(a, 0, f.constant(1)) == f.variable(2));
  // re-rooting shifts cells with every other position (index at 2 here,
  // so every touched position survives the -2 shift)
  CHECK(f.shift_positions(f.array(tab, f.variable(2)), -2) ==
        f.array(std::vector<std::uint32_t>{3, 0, 7, 5}, f.variable(0)));

  // tab[tab[x]]: peel innermost-first
  const iexpr nested = f.array(tab, a);
  CHECK(f.first_subexpr(nested) == a);   // the inner access
  CHECK(f.first_subexpr(a) == x);        // whose index is x
  // x := 1 grounds the inner access to cell position 2; substituting that
  // variable grounds the outer index, which folds the whole chain
  iexpr e = f.subst(nested, 0, f.constant(1));
  CHECK(e == f.array(tab, f.variable(2)));
  e = f.subst(e, 2, f.constant(3));
  CHECK(e == f.variable(7));
}

TEST_CASE("support and print read back") {
  expr_factory f;
  const iexpr x = f.variable(0);
  const iexpr z = f.variable(2);
  const std::uint32_t tab[] = {4, 5, 6, 7};
  const iexpr e = f.add(f.mul(x, f.constant(2)), f.array(tab, z));
  CHECK(f.support(e) == std::vector<std::uint32_t>{0, 2});
  const bexpr g = f.compare(bkind::lt, x, z);
  CHECK(f.support_bool(g) == std::vector<std::uint32_t>{0, 2});
  CHECK(show_bool(f, g) == "(v0 < v2)");
  CHECK(show(f, f.add(x, f.constant(3))) == "(v0 + 3)");
}
