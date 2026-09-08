/// \file petri_props.cc
/// \brief The property tree as surface query atoms (`props_to_surface.hh`).

#include "hsc/petri/props_to_surface.hh"

#include <sstream>
#include <stdexcept>

namespace hsc::petri {

using ::petri::expr::Cmp;
using ::petri::expr::Expression;
using ::petri::expr::LinearAtom;

namespace {

const char* cmp_text(Cmp c) {
  switch (c) {
    case Cmp::EQ: return "==";
    case Cmp::NEQ: return "!=";
    case Cmp::LE: return "<=";
    case Cmp::GE: return ">=";
    case Cmp::LT: return "<";
    case Cmp::GT: return ">";
  }
  return "?";
}

std::string term_text(std::size_t place, long long coeff,
                      const std::vector<std::string>& pnames) {
  if (place >= pnames.size()) {
    throw std::out_of_range("property refers to place index " +
                            std::to_string(place) + " beyond the net");
  }
  if (coeff == 1) return pnames[place];
  return "(* " + std::to_string(coeff) + ' ' + pnames[place] + ')';
}

std::string constant_atom(bool value, const std::vector<std::string>& pnames) {
  if (pnames.empty()) throw std::logic_error("a constant atom needs a leaf");
  return value ? "(>= " + pnames[0] + " 0)" : "(<= " + pnames[0] + " -1)";
}

}  // namespace

std::string linear_form(const LinearAtom& a,
                        const std::vector<std::string>& pnames) {
  if (a.terms.empty()) return "0";
  // right-nested binary sums: (+ t0 (+ t1 t2))
  std::string acc = term_text(a.terms.back().first, a.terms.back().second, pnames);
  for (std::size_t i = a.terms.size() - 1; i-- > 0;) {
    acc = "(+ " + term_text(a.terms[i].first, a.terms[i].second, pnames) +
          ' ' + acc + ')';
  }
  return acc;
}

std::string atom_text(const LinearAtom& a,
                      const std::vector<std::string>& pnames) {
  if (a.terms.empty()) {
    // a comparison between constants: fold it
    const long long lhs = 0;
    bool v = false;
    switch (a.op) {
      case Cmp::EQ: v = lhs == a.constant; break;
      case Cmp::NEQ: v = lhs != a.constant; break;
      case Cmp::LE: v = lhs <= a.constant; break;
      case Cmp::GE: v = lhs >= a.constant; break;
      case Cmp::LT: v = lhs < a.constant; break;
      case Cmp::GT: v = lhs > a.constant; break;
    }
    return constant_atom(v, pnames);
  }
  return std::string("(") + cmp_text(a.op) + ' ' + linear_form(a, pnames) +
         ' ' + std::to_string(a.constant) + ')';
}

std::string query_atom(const Expression& e,
                       const std::vector<std::string>& pnames) {
  using Kind = Expression::Kind;
  switch (e.kind) {
    case Kind::True: return constant_atom(true, pnames);
    case Kind::False: return constant_atom(false, pnames);
    case Kind::Atom: return atom_text(e.atom, pnames);
    case Kind::Not: return "(not " + query_atom(e.children.at(0), pnames) + ')';
    case Kind::And:
    case Kind::Or: {
      if (e.children.size() == 1) return query_atom(e.children[0], pnames);
      std::string out = e.kind == Kind::And ? "(and" : "(or";
      for (const Expression& c : e.children) {
        out += ' ';
        out += query_atom(c, pnames);
      }
      return out + ')';
    }
  }
  throw std::logic_error("unknown expression kind");
}

std::optional<std::string> guard_atom(const SparsePetriNet<int>& net,
                                      std::size_t t) {
  const SparseArray<int>& pre = net.getFlowPT().getColumn(t);
  if (pre.size() == 0) return std::nullopt;
  const std::vector<std::string>& pnames = net.getPnames();
  if (pre.size() == 1) {
    return "(>= " + pnames[pre.keyAt(0)] + ' ' + std::to_string(pre.valueAt(0)) + ')';
  }
  std::string out = "(and";
  for (std::size_t k = 0; k < pre.size(); ++k) {
    out += " (>= " + pnames[pre.keyAt(k)] + ' ' + std::to_string(pre.valueAt(k)) + ')';
  }
  return out + ')';
}

std::optional<std::string> deadlock_atom(const SparsePetriNet<int>& net) {
  const std::size_t n = net.getTransitionCount();
  if (n == 0) return constant_atom(true, net.getPnames());  // nothing fires
  std::string out = "(not (or";
  for (std::size_t t = 0; t < n; ++t) {
    const std::optional<std::string> g = guard_atom(net, t);
    if (!g) return std::nullopt;
    out += ' ';
    out += *g;
  }
  return out + "))";
}

std::string at_least(const LinearAtom& form, long long k,
                     const std::vector<std::string>& pnames) {
  LinearAtom a = form;
  a.op = Cmp::GE;
  a.constant = k;
  return atom_text(a, pnames);
}

}  // namespace hsc::petri
