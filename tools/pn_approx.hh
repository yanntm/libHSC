/// \file pn_approx.hh
/// \brief The invariant set `S ⊇ R` of a net, built in a solver's session from
/// linear facts (`hsc/linear/algorithm.md` §1): the positive P-flows bound the
/// places they cover and constrain every marking; a place never marked is
/// bounded by 0; a NUPN's safe tag bounds every place by 1 and, per unit, its
/// local places by one token in all. `F` names the box, `E_i` the flow
/// equalities, `U_j` the unit constraints, `S` their meet.
///
/// `approx_facts` reads the net, `build_approx` feeds the session.
/// A place no fact bounds keeps its cap: `S` is exact on the covered places
/// (its projection there contains the projection of `R`) and says nothing
/// about the others — a question must not read them.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "hsc/petri/core/SparsePetriNet.h"
#include "hsc/petri/invariants.hh"
#include "hsc/petri/nupn.hh"
#include "pn_solver.hh"

namespace hsc::pn {

struct approx_options {
  int flow_seconds = 5;  ///< budget of the flow computation
  int cap = 2;           ///< domain of an uncovered place: [0, cap)
  bool units = false;    ///< the NUPN unit constraints (needs a present, safe tree)
  bool verbose = false;  ///< the session's lines on stderr
};

struct approx_set {
  std::vector<hsc::petri::pflow> flows;
  std::vector<hsc::petri::pflow> decreasing, increasing;
  bool inequalities = false;
  std::size_t inequality_covered = 0, inequality_tightened = 0;
  std::vector<std::size_t> positive;  ///< indices into `flows`: the semiflows the set uses
  std::vector<long long> bound;       ///< per place: a bound (0 for never marked), -1 when only capped
  std::size_t covered = 0, zeros = 0, unit_constraints = 0;
  bool tagged = false;                ///< a safe NUPN tree: every place bounded by 1
  std::string box_states, set_states, set_nodes;
  double flows_s = 0, set_s = 0;
  [[nodiscard]] bool exact(std::size_t p) const { return bound[p] >= 0; }
};

/// The places that stay empty: no token initially and no producer among the
/// transitions that can fire — a fixpoint over the net alone.
inline std::vector<char> markable_places(const SparsePetriNet<int>& net) {
  const std::size_t np = net.getPlaceCount();
  const MatrixCol<int>& pre = net.getFlowPT();
  const MatrixCol<int>& post = net.getFlowTP();
  const std::vector<int>& marks = net.getMarks();
  std::vector<char> markable(np, 0);
  for (std::size_t p = 0; p < np; ++p) markable[p] = marks[p] > 0;
  for (bool changed = true; changed;) {
    changed = false;
    for (std::size_t t = 0; t < net.getTransitionCount(); ++t) {
      const SparseArray<int>& in = pre.getColumn(t);
      bool live = true;
      for (std::size_t k = 0; k < in.size() && live; ++k) live = markable[in.keyAt(k)];
      if (!live) continue;
      const SparseArray<int>& out = post.getColumn(t);
      for (std::size_t k = 0; k < out.size(); ++k)
        if (!markable[out.keyAt(k)]) { markable[out.keyAt(k)] = 1; changed = true; }
    }
  }
  return markable;
}

/// The linear facts of a net: flows, the bounds they and the tags give,
/// the structural zeros. No session needed — computed before the model is
/// emitted, so the leaf domains can be as wide as the box (the converses
/// are restricted to the declared domains).
inline approx_set approx_facts(const SparsePetriNet<int>& net, const hsc::petri::unit_tree* units, int flow_seconds, bool inequalities = false) {
  using clock = std::chrono::steady_clock;
  approx_set a;
  const std::size_t np = net.getPlaceCount();
  const clock::time_point t0 = clock::now();
  a.inequalities = inequalities;
  if (inequalities) {
    auto found = hsc::petri::pflows_with_inequalities(net, flow_seconds);
    a.flows = std::move(found.equalities);
    a.decreasing = std::move(found.decreasing);
    a.increasing = std::move(found.increasing);
  } else {
    a.flows = hsc::petri::pflows(net, flow_seconds);
  }
  a.bound.assign(np, -1);
  for (std::size_t i = 0; i < a.flows.size(); ++i) {
    const hsc::petri::pflow& f = a.flows[i];
    bool positive = f.constant >= 0;
    for (const auto& [p, c] : f.terms) positive = positive && c > 0;
    if (!positive) continue;
    a.positive.push_back(i);
    for (const auto& [p, c] : f.terms) {
      const long long b = f.constant / c;
      long long& bp = a.bound[static_cast<std::size_t>(p)];
      if (bp < 0 || b < bp) bp = b;
    }
  }
  a.tagged = units != nullptr && units->present() && units->safe;
  if (a.tagged)
    for (long long& b : a.bound) b = b < 0 ? 1 : std::min(b, 1LL);
  const std::vector<char> markable = markable_places(net);
  for (std::size_t p = 0; p < np; ++p) {
    if (!markable[p]) { a.bound[p] = 0; ++a.zeros; }
    if (a.bound[p] >= 0) ++a.covered;
  }
  // Optional decreasing sums improve the box before any place is projected away.
  if (inequalities) {
    const auto previous = a.bound;
    for (const auto& f : a.decreasing)
      for (const auto& [p, c] : f.terms) {
        auto& bound = a.bound[static_cast<std::size_t>(p)];
        const long long candidate = f.constant / c;
        if (bound < 0 || candidate < bound) bound = candidate;
      }
    for (std::size_t p = 0; p < np; ++p) {
      if (previous[p] < 0 && a.bound[p] >= 0) { ++a.inequality_covered; ++a.covered; }
      else if (previous[p] >= 0 && a.bound[p] < previous[p]) ++a.inequality_tightened;
    }
  }
  a.flows_s = std::chrono::duration<double>(clock::now() - t0).count();
  return a;
}

/// The widest bound of the facts, -1 when none.
inline long long widest_bound(const approx_set& a) {
  long long w = -1;
  for (const long long b : a.bound) w = std::max(w, b);
  return w;
}

/// Build `S` (and `F`) in \p s's session from the facts \p a.
inline void build_approx(solver& s, const SparsePetriNet<int>& net, const hsc::petri::unit_tree* units,
                         approx_set& a, const approx_options& o) {
  using clock = std::chrono::steady_clock;
  const std::vector<std::string>& pnames = net.getPnames();
  const std::size_t np = net.getPlaceCount();
  const clock::time_point t1 = clock::now();
  // the box, then the constraints tightest first, then their meet
  std::string full = "(full F";
  for (std::size_t p = 0; p < np; ++p) {
    const long long b = a.bound[p] >= 0 ? a.bound[p] : o.cap - 1;
    full += " (" + pnames[p] + " 0 " + std::to_string(b) + ")";
  }
  for (const std::string& l : s.feed(full + ")")) {
    if (l.rfind("F full ", 0) == 0) a.box_states = l.substr(7);
    else if (o.verbose) std::cerr << l << '\n';
  }
  struct constraint { long long k; std::string form; std::string name; };
  std::vector<constraint> cs;
  for (std::size_t n = 0; n < a.flows.size(); ++n) {  // every flow, the mixed-sign ones included
    const hsc::petri::pflow& f = a.flows[n];
    if (f.terms.empty()) continue;
    std::string eq = " F " + std::to_string(f.constant);
    for (const auto& [p, c] : f.terms) eq += " (* " + std::to_string(c) + ' ' + pnames[static_cast<std::size_t>(p)] + ')';
    cs.push_back({std::llabs(f.constant), "(equality E" + std::to_string(n) + eq + ")", "E" + std::to_string(n)});
  }
  // Both directions use the existing signed at-most constructor. No equality
  // consumer sees these facts. Their support has already survived projection.
  if (a.inequalities) {
    for (bool decreasing : {true, false}) {
      const auto& facts = decreasing ? a.decreasing : a.increasing;
      for (std::size_t n = 0; n < facts.size(); ++n) {
        const auto& f = facts[n];
        const std::string name = std::string(decreasing ? "L" : "G") + std::to_string(n);
        std::string form = "(at-most " + name + " F " + std::to_string(decreasing ? f.constant : -f.constant);
        for (const auto& [p, c] : f.terms)
          form += " (* " + std::to_string(decreasing ? static_cast<long long>(c) : -static_cast<long long>(c))
               + ' ' + pnames[static_cast<std::size_t>(p)] + ')';
        cs.push_back({f.constant, form + ')', name});
      }
    }
  }
  if (o.units && a.tagged && units != nullptr) {
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t p = 0; p < np; ++p) index.emplace(pnames[p], p);
    std::size_t n = 0;
    for (const auto& [id, u] : units->units) {
      std::string terms;
      std::size_t k = 0;
      for (const std::string& pl : u.places)
        if (index.count(pl)) { terms += " (* 1 " + pl + ')'; ++k; }
      if (k < 2) continue;  // one place: the safe bound says it already
      cs.push_back({1, "(at-most U" + std::to_string(n) + " F 1" + terms + ")", "U" + std::to_string(n)});
      ++n;
    }
    a.unit_constraints = n;
  }
  std::stable_sort(cs.begin(), cs.end(), [](const constraint& x, const constraint& y) { return x.k < y.k; });
  std::string sel = "(intersect S F";
  for (const constraint& c : cs) {
    for (const std::string& l : s.feed(c.form)) if (o.verbose) std::cerr << l << '\n';
    sel += ' ' + c.name;
  }
  for (const std::string& l : s.feed(sel + ")")) if (o.verbose) std::cerr << l << '\n';
  for (const std::string& l : s.feed("(count S) (nodes S)")) {
    if (l.rfind("S count ", 0) == 0) a.set_states = l.substr(8);
    else if (l.rfind("S nodes ", 0) == 0) a.set_nodes = l.substr(8);
    else if (o.verbose) std::cerr << l << '\n';
  }
  a.set_s = std::chrono::duration<double>(clock::now() - t1).count();
}

/// One line of statistics for the logs.
inline void print_approx_stats(std::ostream& out, const approx_set& a, std::size_t np) {
  if (a.inequalities)
    out << "hsc-pn: approx inequalities decreasing=" << a.decreasing.size()
        << " increasing=" << a.increasing.size() << " newly_covered=" << a.inequality_covered
        << " tightened=" << a.inequality_tightened << '\n';
  out << "hsc-pn: approx flows=" << a.flows.size() << " positive=" << a.positive.size()
      << " covered=" << a.covered << "/" << np << " zeros=" << a.zeros << " units=" << a.unit_constraints
      << " box=" << a.box_states << " set=" << a.set_states << " set_nodes=" << a.set_nodes
      << " flows_s=" << a.flows_s << " set_s=" << a.set_s << '\n';
}

}  // namespace hsc::pn
