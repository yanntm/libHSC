/// \file forward.cc
/// \brief The forward conversion rules (`hsc/ctl/forward.hh`,
/// `algorithm.md` §4–§5).
#include "hsc/ctl/forward.hh"

#include <algorithm>

#include "hsc/util/hash.hh"

namespace hsc::ctl {

std::size_t forward::set_hasher::operator()(const set_expr& e) const noexcept {
  std::size_t seed = util::hash_value(static_cast<std::uint8_t>(e.kind));
  util::hash_combine(seed, e.arg);
  util::hash_combine(seed, e.f);
  return seed;
}

std::size_t forward::q_hasher::operator()(const question& q) const noexcept {
  std::size_t seed = util::hash_value(static_cast<std::uint8_t>(q.kind));
  util::hash_combine(seed, q.set);
  util::hash_range(seed, q.kids.begin(), q.kids.end());
  return seed;
}

set_id forward::mk_set(set_expr e) {
  if (const auto it = set_index_.find(e); it != set_index_.end())
    return it->second;
  const set_id id = static_cast<set_id>(sets_.size());
  sets_.push_back(e);
  set_index_.emplace(e, id);
  return id;
}

q_id forward::mk_q(question q) {
  if (const auto it = q_index_.find(q); it != q_index_.end())
    return it->second;
  const q_id id = static_cast<q_id>(qs_.size());
  qs_.push_back(q);
  q_index_.emplace(std::move(q), id);
  return id;
}

q_id forward::nonempty(set_id s) { return mk_q({q_op::nonempty, s, {}}); }

q_id forward::any(std::vector<q_id> kids) {
  // Flatten, drop `never`, deduplicate; a singleton is its child.
  std::vector<q_id> flat;
  for (const q_id k : kids) {
    const question& q = qs_[k];
    if (q.kind == q_op::never) continue;
    if (q.kind == q_op::any)
      flat.insert(flat.end(), q.kids.begin(), q.kids.end());
    else
      flat.push_back(k);
  }
  std::ranges::sort(flat);
  const auto dup = std::ranges::unique(flat);
  flat.erase(dup.begin(), dup.end());
  if (flat.empty()) return mk_q({q_op::never, 0, {}});
  if (flat.size() == 1) return flat.front();
  return mk_q({q_op::any, 0, std::move(flat)});
}

q_id forward::rule(set_id r, node_id phi) {
  const fnode n = f_[phi];  // copy: the tables below may grow
  // A state formula: the seed filtered, asked.
  if (f_.is_state(phi)) {
    if (n.kind == op::fls) return mk_q({q_op::never, 0, {}});
    if (n.kind == op::tru) return nonempty(r);
    return nonempty(mk_set({set_op::filter, r, phi}));
  }
  switch (n.kind) {
    case op::ex:
      return rule(mk_set({set_op::ey, r, 0}), n.kids[0]);
    case op::ef:
      return rule(mk_set({set_op::fwdu, r, f_.constant(true)}), n.kids[0]);
    case op::eu:
      return rule(mk_set({set_op::fwdu, r, n.kids[0]}), n.kids[1]);
    case op::eg:
      return nonempty(mk_set({set_op::fwdg, r, n.kids[0]}));
    case op::ew: {
      // E[q W f] = E[q U f] ∨ EG q
      const q_id until = rule(mk_set({set_op::fwdu, r, n.kids[0]}), n.kids[1]);
      const q_id glob = nonempty(mk_set({set_op::fwdg, r, n.kids[0]}));
      return any({until, glob});
    }
    case op::or_: {
      std::vector<q_id> ks;
      ks.reserve(n.kids.size());
      for (const node_id k : n.kids) ks.push_back(rule(r, k));
      return any(std::move(ks));
    }
    case op::and_: {
      // State-formula children join the seed as one filter; among the
      // rest, the last convertible child (VIS: the right conjunct) continues
      // forward and every other child restricts the seed by its backward
      // Sat. Which conjunct goes forward is a choice to re-examine.
      std::vector<node_id> state;
      std::vector<node_id> rest;
      for (const node_id k : n.kids)
        (f_.is_state(k) ? state : rest).push_back(k);
      set_id cur = r;
      if (!state.empty()) cur = mk_set({set_op::filter, cur, f_.conj(state)});
      node_id fwd = 0;
      bool have_fwd = false;
      for (auto it = rest.rbegin(); it != rest.rend(); ++it) {
        if (!have_fwd && f_.convertible(*it)) {
          fwd = *it;
          have_fwd = true;
        } else {
          cur = mk_set({set_op::restrict_, cur, *it});
        }
      }
      return have_fwd ? rule(cur, fwd) : nonempty(cur);
    }
    default:
      // A universal operator under the seed: evaluated backward.
      return nonempty(mk_set({set_op::restrict_, r, phi}));
  }
}

forward_form forward::convert(node_id phi, bool single_initial) {
  const node_id nf = f_.nnf(phi);
  const set_id init = mk_set({set_op::init, 0, 0});
  if (single_initial && f_.convertible(nf)) {
    return {rule(init, nf), false};
  }
  // Ask `I ∧ ¬φ ≠ ∅`; the verdict is the complement.
  return {rule(init, f_.negate(nf)), true};
}

std::string forward::print_set(
    set_id s,
    const std::function<std::string(std::uint32_t)>& atom_name) const {
  const set_expr& e = sets_[s];
  switch (e.kind) {
    case set_op::init: return "init";
    case set_op::ey: return "(ey " + print_set(e.arg, atom_name) + ")";
    case set_op::filter:
      return "(filter " + print_set(e.arg, atom_name) + " " +
             f_.print(e.f, atom_name) + ")";
    case set_op::fwdu:
      return "(fwdu " + print_set(e.arg, atom_name) + " " +
             f_.print(e.f, atom_name) + ")";
    case set_op::fwdg:
      return "(fwdg " + print_set(e.arg, atom_name) + " " +
             f_.print(e.f, atom_name) + ")";
    case set_op::restrict_:
      return "(restrict " + print_set(e.arg, atom_name) + " " +
             f_.print(e.f, atom_name) + ")";
  }
  return "?";
}

std::string forward::print_q(
    q_id i,
    const std::function<std::string(std::uint32_t)>& atom_name) const {
  const question& q = qs_[i];
  switch (q.kind) {
    case q_op::never: return "never";
    case q_op::nonempty: return "(nonempty " + print_set(q.set, atom_name) + ")";
    case q_op::any: {
      std::string s = "(any";
      for (const q_id k : q.kids) s += " " + print_q(k, atom_name);
      return s + ")";
    }
  }
  return "?";
}

}  // namespace hsc::ctl
