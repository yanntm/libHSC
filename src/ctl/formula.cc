/// \file formula.cc
/// \brief The formula DAG: interning with normalisation, negation normal
/// form, the structural queries (`hsc/ctl/formula.hh`).
#include "hsc/ctl/formula.hh"

#include <algorithm>
#include <cassert>

#include "hsc/util/hash.hh"

namespace hsc::ctl {

const char* name(op o) noexcept {
  switch (o) {
    case op::atom: return "atom";
    case op::tru: return "true";
    case op::fls: return "false";
    case op::not_: return "not";
    case op::and_: return "and";
    case op::or_: return "or";
    case op::ex: return "EX";
    case op::ax: return "AX";
    case op::ef: return "EF";
    case op::af: return "AF";
    case op::eg: return "EG";
    case op::ag: return "AG";
    case op::eu: return "EU";
    case op::au: return "AU";
    case op::ew: return "EW";
    case op::aw: return "AW";
  }
  return "?";
}

std::size_t formulas::hasher::operator()(const fnode& n) const noexcept {
  std::size_t seed = util::hash_value(static_cast<std::uint8_t>(n.kind));
  util::hash_combine(seed, n.atom);
  util::hash_range(seed, n.kids.begin(), n.kids.end());
  return seed;
}

node_id formulas::make(fnode n) {
  if (const auto it = index_.find(n); it != index_.end()) return it->second;
  const node_id id = static_cast<node_id>(nodes_.size());
  nodes_.push_back(n);
  index_.emplace(std::move(n), id);
  return id;
}

node_id formulas::atom(std::uint32_t a) { return make({op::atom, a, {}}); }

node_id formulas::constant(bool b) {
  return make({b ? op::tru : op::fls, 0, {}});
}

node_id formulas::negation(node_id f) {
  const fnode& n = nodes_[f];
  switch (n.kind) {
    case op::tru: return constant(false);
    case op::fls: return constant(true);
    case op::not_: return n.kids[0];
    default: return make({op::not_, 0, {f}});
  }
}

node_id formulas::nary(op o, std::span<const node_id> kids) {
  // Flatten same-operator children, fold constants, deduplicate.
  const bool is_and = o == op::and_;
  std::vector<node_id> flat;
  for (const node_id k : kids) {
    const fnode& n = nodes_[k];
    if (n.kind == o) {
      flat.insert(flat.end(), n.kids.begin(), n.kids.end());
    } else if (n.kind == (is_and ? op::tru : op::fls)) {
      continue;  // neutral
    } else if (n.kind == (is_and ? op::fls : op::tru)) {
      return constant(!is_and);  // absorbing
    } else {
      flat.push_back(k);
    }
  }
  std::ranges::sort(flat);
  const auto dup = std::ranges::unique(flat);
  flat.erase(dup.begin(), dup.end());
  if (flat.empty()) return constant(is_and);
  if (flat.size() == 1) return flat.front();
  return make({o, 0, std::move(flat)});
}

node_id formulas::conj(std::span<const node_id> kids) {
  return nary(op::and_, kids);
}
node_id formulas::disj(std::span<const node_id> kids) {
  return nary(op::or_, kids);
}

node_id formulas::unary(op o, node_id f) {
  assert(is_path(o) && o <= op::ag && "unary path operator expected");
  return make({o, 0, {f}});
}

node_id formulas::binary(op o, node_id left, node_id right) {
  assert(o >= op::eu && "binary path operator expected");
  return make({o, 0, {left, right}});
}

node_id formulas::nnf(node_id f) {
  if (const auto it = nnf_memo_.find(f); it != nnf_memo_.end())
    return it->second;
  const fnode n = nodes_[f];  // copy: interning below may grow nodes_
  node_id r = f;
  switch (n.kind) {
    case op::atom:
    case op::tru:
    case op::fls:
      break;
    case op::not_:
      r = negate(n.kids[0]);
      break;
    case op::and_:
    case op::or_: {
      std::vector<node_id> ks;
      ks.reserve(n.kids.size());
      for (const node_id k : n.kids) ks.push_back(nnf(k));
      r = nary(n.kind, ks);
      break;
    }
    default: {
      std::vector<node_id> ks;
      for (const node_id k : n.kids) ks.push_back(nnf(k));
      r = ks.size() == 1 ? unary(n.kind, ks[0])
                         : binary(n.kind, ks[0], ks[1]);
      break;
    }
  }
  nnf_memo_.emplace(f, r);
  return r;
}

node_id formulas::negate(node_id f) {
  if (const auto it = neg_memo_.find(f); it != neg_memo_.end())
    return it->second;
  const fnode n = nodes_[f];
  node_id r = f;
  switch (n.kind) {
    case op::atom: r = negation(f); break;
    case op::tru: r = constant(false); break;
    case op::fls: r = constant(true); break;
    case op::not_: r = nnf(n.kids[0]); break;
    case op::and_:
    case op::or_: {
      std::vector<node_id> ks;
      ks.reserve(n.kids.size());
      for (const node_id k : n.kids) ks.push_back(negate(k));
      r = nary(n.kind == op::and_ ? op::or_ : op::and_, ks);
      break;
    }
    case op::ex: r = unary(op::ax, negate(n.kids[0])); break;
    case op::ax: r = unary(op::ex, negate(n.kids[0])); break;
    case op::ef: r = unary(op::ag, negate(n.kids[0])); break;
    case op::ag: r = unary(op::ef, negate(n.kids[0])); break;
    case op::eg: r = unary(op::af, negate(n.kids[0])); break;
    case op::af: r = unary(op::eg, negate(n.kids[0])); break;
    case op::eu:
    case op::au:
    case op::ew:
    case op::aw: {
      // ¬E[a U b] = A[¬b W (¬a ∧ ¬b)]   ¬A[a U b] = E[¬b W (¬a ∧ ¬b)]
      // ¬E[a W b] = A[¬b U (¬a ∧ ¬b)]   ¬A[a W b] = E[¬b U (¬a ∧ ¬b)]
      const node_id na = negate(n.kids[0]);
      const node_id nb = negate(n.kids[1]);
      const node_id both = conj(na, nb);
      const op dual = n.kind == op::eu   ? op::aw
                      : n.kind == op::au ? op::ew
                      : n.kind == op::ew ? op::au
                                         : op::eu;
      r = binary(dual, nb, both);
      break;
    }
  }
  neg_memo_.emplace(f, r);
  return r;
}

bool formulas::is_state(node_id f) const {
  const fnode& n = nodes_[f];
  if (is_path(n.kind)) return false;
  for (const node_id k : n.kids)
    if (!is_state(k)) return false;
  return true;
}

formulas::quantifiers formulas::path_quantifiers(node_id f) const {
  const fnode& n = nodes_[f];
  quantifiers q = quantifiers::none;
  if (is_path(n.kind)) q = is_existential(n.kind) ? quantifiers::existential : quantifiers::universal;
  for (const node_id k : n.kids) {
    const quantifiers kq = path_quantifiers(k);
    if (kq == quantifiers::none) continue;
    if (q == quantifiers::none) q = kq;
    else if (q != kq) return quantifiers::mixed;
  }
  return q;
}

bool formulas::convertible(node_id f) const {
  const fnode& n = nodes_[f];
  if (is_state(f)) return true;
  if (is_existential(n.kind)) return true;
  if (n.kind == op::and_ || n.kind == op::or_) {
    for (const node_id k : n.kids)
      if (!convertible(k)) return false;
    return true;
  }
  return false;
}

std::string formulas::print(
    node_id f,
    const std::function<std::string(std::uint32_t)>& atom_name) const {
  const fnode& n = nodes_[f];
  switch (n.kind) {
    case op::atom: return atom_name(n.atom);
    case op::tru: return "true";
    case op::fls: return "false";
    default: break;
  }
  std::string s = "(";
  s += name(n.kind);
  for (const node_id k : n.kids) {
    s += ' ';
    s += print(k, atom_name);
  }
  s += ')';
  return s;
}

}  // namespace hsc::ctl
