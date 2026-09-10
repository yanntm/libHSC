/// \file test_ctl.cc
/// \brief The CTL formula DAG and the forward conversion on hand formulas:
/// normal forms are checked as identities, the forward form against the
/// rules of `include/hsc/ctl/algorithm.md` §4–§5 rendered as text.

#include <doctest/doctest.h>

#include <string>

#include "hsc/ctl/formula.hh"
#include "hsc/ctl/forward.hh"

using namespace hsc::ctl;

namespace {

std::string atom_name(std::uint32_t a) { return std::string(1, char('a' + a)); }

struct fx {
  formulas F;
  node_id a = F.atom(0), b = F.atom(1), c = F.atom(2);
  std::string s(node_id n) const { return F.print(n, atom_name); }
};

}  // namespace

TEST_CASE("the DAG interns: equal subformulas are one node") {
  fx x;
  const node_id l = x.F.unary(op::ef, x.F.conj(x.a, x.b));
  const node_id r = x.F.unary(op::ef, x.F.conj(x.b, x.a));
  CHECK(l == r);
  CHECK(x.F.conj(x.a, x.F.constant(true)) == x.a);
  CHECK(x.F.disj(x.a, x.F.constant(true)) == x.F.constant(true));
  CHECK(x.F.negation(x.F.negation(x.a)) == x.a);
}

TEST_CASE("negation normal form pushes not to the atoms through the duals") {
  fx x;
  const node_id f = x.F.negation(x.F.unary(op::ag, x.F.disj(x.a, x.F.unary(op::ex, x.b))));
  CHECK(x.s(x.F.nnf(f)) == "(EF (and (not a) (AX (not b))))");
  const node_id u = x.F.negation(x.F.binary(op::eu, x.a, x.b));
  CHECK(x.s(x.F.nnf(u)) == "(AW (not b) (and (not a) (not b)))");
  const node_id w = x.F.negation(x.F.binary(op::aw, x.a, x.b));
  CHECK(x.s(x.F.nnf(w)) == "(EU (not b) (and (not a) (not b)))");
  // nnf is idempotent and negate is an involution on NNF
  const node_id n = x.F.nnf(f);
  CHECK(x.F.nnf(n) == n);
  CHECK(x.F.negate(x.F.negate(n)) == n);
}

TEST_CASE("path quantifiers of a formula in NNF") {
  fx x;
  using Q = formulas::quantifiers;
  CHECK(x.F.path_quantifiers(x.F.conj(x.a, x.F.negation(x.b))) == Q::none);
  CHECK(x.F.path_quantifiers(x.F.unary(op::ef, x.F.conj(x.a, x.F.unary(op::ex, x.b)))) == Q::existential);
  CHECK(x.F.path_quantifiers(x.F.binary(op::au, x.a, x.F.unary(op::ag, x.b))) == Q::universal);
  CHECK(x.F.path_quantifiers(x.F.unary(op::ef, x.F.unary(op::ag, x.b))) == Q::mixed);
  // the dual of an existential formula is universal: what stands as TRUE
  // on one side stands as FALSE on the other
  const node_id f = x.F.unary(op::ef, x.F.disj(x.a, x.F.unary(op::eg, x.b)));
  CHECK(x.F.path_quantifiers(x.F.nnf(x.F.negation(f))) == Q::universal);
}

TEST_CASE("state formulas and convertibility") {
  fx x;
  CHECK(x.F.is_state(x.F.conj(x.a, x.F.negation(x.b))));
  CHECK_FALSE(x.F.is_state(x.F.unary(op::ex, x.a)));
  CHECK(x.F.convertible(x.F.unary(op::ef, x.F.unary(op::ag, x.a))));  // E-led
  CHECK_FALSE(x.F.convertible(x.F.unary(op::ag, x.a)));
  CHECK(x.F.convertible(x.F.disj(x.F.unary(op::ex, x.a), x.b)));
  CHECK_FALSE(x.F.convertible(x.F.disj(x.F.unary(op::ax, x.a), x.b)));
}

TEST_CASE("forward form: the existential operators become images and closures") {
  fx x;
  forward fw(x.F);
  auto pq = [&](node_id phi) {
    const forward_form ff = fw.convert(phi);
    return (ff.negated ? "!" : "") + fw.print_q(ff.root, atom_name);
  };
  // EF a: reach from init through anything, then filter by a
  CHECK(pq(x.F.unary(op::ef, x.a)) == "(nonempty (filter (fwdu init true) a))");
  // EX EG b
  CHECK(pq(x.F.unary(op::ex, x.F.unary(op::eg, x.b))) ==
        "(nonempty (fwdg (ey init) b))");
  // E[a U b]
  CHECK(pq(x.F.binary(op::eu, x.a, x.b)) == "(nonempty (filter (fwdu init a) b))");
  // E[a W b] = E[a U b] ∨ EG a
  CHECK(pq(x.F.binary(op::ew, x.a, x.b)) ==
        "(any (nonempty (filter (fwdu init a) b)) (nonempty (fwdg init a)))");
  // disjunction splits the question; conjunction with a state formula filters
  // children of `any` are ordered by question id, i.e. by creation
  const node_id exa = x.F.unary(op::ex, x.a);
  const node_id efb = x.F.unary(op::ef, x.b);
  CHECK(pq(x.F.disj(exa, efb)) ==
        "(any (nonempty (filter (ey init) a)) (nonempty (filter (fwdu init true) b)))");
  CHECK(pq(x.F.conj(x.c, x.F.unary(op::ex, x.a))) ==
        "(nonempty (filter (ey (filter init c)) a))");
}

TEST_CASE("forward form: universal operators under the seed restrict backward") {
  fx x;
  forward fw(x.F);
  auto pq = [&](node_id phi) {
    const forward_form ff = fw.convert(phi);
    return (ff.negated ? "!" : "") + fw.print_q(ff.root, atom_name);
  };
  // AG a is not convertible: ask I ∧ EF ¬a ≠ ∅ and negate
  CHECK(pq(x.F.unary(op::ag, x.a)) == "!(nonempty (filter (fwdu init true) (not a)))");
  // EF AG a: forward to the AG, then backward for it
  CHECK(pq(x.F.unary(op::ef, x.F.unary(op::ag, x.a))) ==
        "(nonempty (restrict (fwdu init true) (AG a)))");
  // two convertible conjuncts: the last (by node id, VIS's right operand)
  // goes forward, the other restricts
  const node_id exa = x.F.unary(op::ex, x.a);
  const node_id efb = x.F.unary(op::ef, x.b);
  CHECK(exa < efb);
  CHECK(pq(x.F.conj(exa, efb)) ==
        "(nonempty (filter (fwdu (restrict init (EX a)) true) b))");
  // several initial states: always the negated question
  const forward_form ff = fw.convert(x.F.unary(op::ef, x.a), false);
  CHECK(ff.negated);
  CHECK(fw.print_q(ff.root, atom_name) == "(nonempty (restrict init (AG (not a))))");
}
