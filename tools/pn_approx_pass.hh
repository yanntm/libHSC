/// \file pn_approx_pass.hh
/// \brief The over-approximation pass of `hsc-pn`, in a session of its own:
/// the net abstracted to the places the linear facts bound (`pn_abstract.hh`),
/// its model emitted under the Sloan order with leaf domains as wide as the
/// box, the invariant set `S` built (`pn_approx.hh`), then the questions —
/// the properties `S` refutes, the backward searches inside `S`, the dead
/// transitions. Everything here is sound for the original net on the
/// places kept: a question that reads a removed place is not asked.
#pragma once
#include <chrono>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "hsc/order/bandwidth.hh"
#include "hsc/petri/decompose.hh"
#include "hsc/petri/expr/Property.h"
#include "hsc/petri/nupn.hh"
#include "hsc/petri/to_surface.hh"
#include "pn_abstract.hh"
#include "pn_approx.hh"
#include "pn_approx_bounds.hh"
#include "pn_solver.hh"

namespace hsc::pn {

struct approx_pass_options {
  int flow_seconds = 5;
  int cap = 2;              ///< the leaf domain of the original model, [0, cap)
  bool units = false;       ///< the NUPN unit constraints
  bool inequalities = false; ///< harvest monotone sums in the same flow run
  std::size_t back = 0;     ///< layers of the backward search per open property (0: none)
  double back_time = 2.0;   ///< seconds per backward search
  bool dead = false;        ///< the dead-transition report instead of the properties
  bool dead_step = false;   ///< with dead, the one-step test too
  std::size_t dead_depth = 1;  ///< layers of the backward search from a slice
  int dead_budget = 0;      ///< with dead, seconds for the tests (0: none); the verdicts taken stand
  int dead_gfp = -1;        ///< with dead: after the tests, one image of S under the live events, then the forward gfp (rounds; 0: unbounded; -1: off)
  bool verbose = false;
};

struct approx_pass_report {
  std::size_t removed = 0, refuted = 0, open_before = 0, skipped_removed = 0, test_timeouts = 0;
  std::size_t back_decided = 0, back_init = 0, back_closed = 0, back_open = 0, back_partial = 0;
  double tests_s = 0, back_s = 0;
  std::vector<std::string> dead_names;  ///< with dead: the names of the dead transitions
  std::size_t never = 0, step = 0, alive = 0, untested = 0, candidates = 0;
  bool dead_stopped = false;
  double dead_s = 0;
  std::string dead_line;                ///< with dead: the DEAD_TRANSITIONS summary line
};

/// Run the pass. \p open is one flag per property, cleared for what is
/// answered (FORMULA lines on \p out). \p tags is the NUPN tree of the
/// original net (its unit places are matched by name).
inline approx_pass_report run_approx_pass(const SparsePetriNet<int>& net, const hsc::petri::unit_tree& tags,
                                          const std::vector<::petri::expr::Property>& properties,
                                          std::vector<char>& open, const approx_pass_options& o,
                                          std::ostream& out) {
  using clock = std::chrono::steady_clock;
  const auto sec = [](clock::time_point a, clock::time_point b) { return std::chrono::duration<double>(b - a).count(); };
  approx_pass_report rep;
  const clock::time_point t0 = clock::now();
  // the facts on the original net; the abstraction keeps the bounded places
  approx_set facts = approx_facts(net, tags.present() ? &tags : nullptr, o.flow_seconds, o.inequalities);
  std::vector<char> keep(net.getPlaceCount(), 0);
  for (std::size_t p = 0; p < keep.size(); ++p) keep[p] = facts.bound[p] >= 0;
  const abstraction abs = abstract_net(net, keep);
  rep.removed = abs.removed;
  if (abs.kept.empty()) {
    // no place bounded by a flow: the set is every marking, nothing is refuted, nothing is dead
    rep.alive = net.getTransitionCount();
    rep.dead_line = "DEAD_TRANSITIONS 0 of " + std::to_string(net.getTransitionCount()) + " (no place bounded by a flow)";
    return rep;
  }
  const std::vector<long long> orig_bound = facts.bound;  // by original place, for the questions
  // the facts renumbered to the abstract net (every place exact there)
  approx_set afacts = facts;
  afacts.bound.clear();
  for (const std::size_t p : abs.kept) afacts.bound.push_back(facts.bound[p]);
  afacts.covered = abs.kept.size();
  // a flow through a removed place is no constraint of the abstract net
  afacts.flows.clear();
  afacts.positive.clear();
  for (const hsc::petri::pflow& f : facts.flows) {
    bool kept_flow = true;
    for (const auto& [p, c] : f.terms) kept_flow = kept_flow && keep[static_cast<std::size_t>(p)];
    if (!kept_flow) continue;
    hsc::petri::pflow g = f;
    bool positive = g.constant >= 0;
    for (auto& [p, c] : g.terms) {
      p = static_cast<int>(abs.to_abstract[static_cast<std::size_t>(p)]);
      positive = positive && c > 0;
    }
    if (positive) afacts.positive.push_back(afacts.flows.size());
    afacts.flows.push_back(std::move(g));
  }
  if (o.inequalities) {
    // Keep whole supports only, especially for lower bounds: dropping a term
    // from sum >= K is unsound. Decreasing supports are bounded by construction.
    const auto project = [&](const std::vector<hsc::petri::pflow>& source) {
      std::vector<hsc::petri::pflow> target;
      for (const auto& f : source) {
        bool retained = true;
        for (const auto& [p, c] : f.terms) retained = retained && keep[static_cast<std::size_t>(p)];
        if (!retained) continue;
        auto g = f;
        for (auto& [p, c] : g.terms) p = static_cast<int>(abs.to_abstract[static_cast<std::size_t>(p)]);
        target.push_back(std::move(g));
      }
      return target;
    };
    afacts.decreasing = project(facts.decreasing);
    afacts.increasing = project(facts.increasing);
  }
  const SparsePetriNet<int>& anet = abs.net;
  // the model of the abstract net: the Sloan order of the *original* net
  // projected onto the kept places (the removed places carry dependency
  // structure the order needs — BugTracking's set has 8249 nodes under the
  // projected order, 39 587 under an order computed on the abstract net),
  // domains as wide as the box
  const std::vector<hsc::order::louvain::edge> edges = hsc::petri::dependency_edges(net);
  const std::vector<std::uint32_t> full_order = hsc::order::sloan(static_cast<int>(net.getPlaceCount()), edges);
  std::vector<std::uint32_t> listing;
  listing.reserve(abs.kept.size());
  for (const std::uint32_t p : full_order)
    if (keep[p]) listing.push_back(static_cast<std::uint32_t>(abs.to_abstract[p]));
  const hsc::petri::unit_tree units = hsc::petri::ordered(anet, listing);
  hsc::petri::emit_options eo;
  eo.exam = hsc::petri::examination::model_only;
  eo.skip_no_effect = !o.dead;
  int domain = o.cap;
  for (int m : anet.getMarks()) domain = std::max(domain, m + 1);
  const long long w = widest_bound(afacts);
  if (w >= 0 && w + 1 > domain) domain = static_cast<int>(std::min<long long>(w + 1, 1 << 30));
  eo.bound = domain;
  // A single kept place must still have a product root for the linear-set
  // surface reader (which reads domains from product arcs). The unit tail
  // adds no variable or state; a bare leaf root is not supported there.
  if (o.inequalities && abs.kept.size() == 1)
    eo.shape_form = "(spine " + anet.getPnames().front() + ")";
  std::ostringstream model;
  hsc::petri::to_surface(model, anet, units, eo);
  if (o.dead) model << "(print-spec)\n";
  solver s(anet, domain, o.verbose);
  s.set_property_names(net.getPnames());
  std::vector<std::string> fed_lines;
  try {
    fed_lines = s.feed(model.str());
  } catch (const hsc::surface::translate_error& e) {
    // the model the abstract net produced is not well formed: say which line
    std::istringstream in(model.str());
    std::string line, head;
    for (int i = 1; i <= 4 && std::getline(in, line); ++i) head += "\n  " + std::to_string(i) + ": " + line.substr(0, 160);
    throw std::runtime_error(std::string(e.what()) + " in the approx model of " + std::to_string(anet.getPlaceCount()) + " places, " + std::to_string(anet.getTransitionCount()) + " transitions" + head);
  }
  for (const std::string& l : fed_lines)
    if (o.verbose && (l.empty() || l.front() != '(')) std::cerr << l << '\n';
  approx_options ao;
  ao.flow_seconds = o.flow_seconds;
  ao.cap = o.cap;
  ao.units = o.units;
  ao.verbose = o.verbose;
  build_approx(s, anet, tags.present() ? &tags : nullptr, afacts, ao);
  print_approx_stats(std::cerr, afacts, net.getPlaceCount());
  if (rep.removed != 0) std::cerr << "hsc-pn: approx abstraction removed " << rep.removed << " uncovered places\n";

  if (o.dead) {
    const clock::time_point t2 = clock::now();
    if (o.dead_budget > 0) s.set_deadline(t2 + std::chrono::seconds(o.dead_budget));
    const std::string form = o.dead_step ? "(dead D S step " + std::to_string(o.dead_depth) + ")" : "(dead D S)";
    for (const std::string& l : s.feed(form)) {
      if (l.rfind("D dead ", 0) == 0) {
        const std::string rest = l.substr(7);
        rep.dead_names.push_back(rest.substr(0, rest.find(' ')));
        if (rest.find(" never") != std::string::npos) ++rep.never; else ++rep.step;
      } else if (l.rfind("D dead-summary", 0) == 0) {
        std::istringstream in(l.substr(15));
        std::string k; std::size_t v;
        rep.dead_stopped = l.find(" stopped") != std::string::npos;
        while (in >> k >> v) { if (k == "alive") rep.alive = v; else if (k == "untested") rep.untested = v; else if (k == "candidates") rep.candidates = v; }
      } else if (o.verbose) {
        std::cerr << l << '\n';
      }
    }
    s.set_deadline(std::nullopt);
    rep.dead_s = sec(t2, clock::now());
    if (o.dead_gfp >= 0) {
      // the cost of one step of the live transition relation on S, then the
      // forward gfp over it (`(support …)`), each under the same budget
      const clock::time_point t3 = clock::now();
      if (o.dead_budget > 0) s.set_deadline(t3 + std::chrono::seconds(o.dead_budget));
      for (const std::string& l : s.feed("(post I S S alive D)"))
        if (l.rfind("I ", 0) == 0) std::cerr << "hsc-pn: image " << l.substr(2) << " in " << sec(t3, clock::now()) << " s\n";
      s.set_deadline(std::nullopt);
      const clock::time_point t4 = clock::now();
      if (o.dead_budget > 0) s.set_deadline(t4 + std::chrono::seconds(o.dead_budget));
      for (const std::string& l : s.feed("(support G S alive D rounds " + std::to_string(o.dead_gfp) + ")"))
        if (l.rfind("G ", 0) == 0) std::cerr << "hsc-pn: gfp " << l.substr(2) << " in " << sec(t4, clock::now()) << " s\n";
      s.set_deadline(std::nullopt);
    }
    std::ostringstream line;
    line << "DEAD_TRANSITIONS " << (rep.never + rep.step) << " of " << net.getTransitionCount() << " (never " << rep.never
         << ", one step " << rep.step << ", alive " << rep.alive << ", untested " << rep.untested
         << ", candidates " << rep.candidates << (rep.dead_stopped ? ", stopped" : "") << ")"
         << " flows " << afacts.flows.size() << " positive " << afacts.positive.size() << " covered places "
         << afacts.covered << " of " << net.getPlaceCount() << " (never marked " << afacts.zeros
         << ", unit constraints " << afacts.unit_constraints << ", removed " << rep.removed << ")"
         << " box " << afacts.box_states << " set " << afacts.set_states << " states"
         << " times flows " << afacts.flows_s << " s, set " << afacts.set_s << " s, tests " << rep.dead_s << " s";
    rep.dead_line = line.str();
    return rep;
  }

  // the properties S refutes (a goal reading a removed place is left open)
  const clock::time_point ta = clock::now();
  for (std::size_t i = 0; i < properties.size(); ++i) {
    if (!open[i]) continue;
    ++rep.open_before;
    if (rep.removed != 0 && properties[i].kind == ::petri::expr::PropertyKind::Deadlock) { ++rep.skipped_removed; continue; }
    if (solver::reads_removed(properties[i], orig_bound)) { ++rep.skipped_removed; continue; }
    if (o.inequalities && properties[i].kind == ::petri::expr::PropertyKind::Bound) {
      if (bound_from_approx(s, properties[i], net, o.back_time, out)) {
        open[i] = 0;
        ++rep.refuted;
      }
      continue;
    }
    if (s.refute(properties[i], "S", orig_bound, out, o.back_time)) {
      open[i] = 0;
      ++rep.refuted;
      if (o.verbose) std::cerr << "hsc-pn: refuted " << properties[i].name << " on S\n";
    }
  }
  const clock::time_point tb = clock::now();
  rep.tests_s = sec(ta, tb);
  if (o.back > 0) {
    for (std::size_t i = 0; i < properties.size(); ++i) {
      if (!open[i]) continue;
      s.set_deadline(clock::now() + std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(o.back_time)));
      std::string how;
      // every place of the abstract net is exact in S; its paths are the
      // original net's only when nothing was removed
      const bool done = s.refute_back(properties[i], "S", orig_bound, /*all_exact=*/true,
                                      /*exact_net=*/rep.removed == 0, o.back, out, &how);
      s.set_deadline(std::nullopt);
      if (how.rfind("init", 0) == 0) ++rep.back_init; else if (how.rfind("closed", 0) == 0) ++rep.back_closed;
      else if (how.rfind("open", 0) == 0) ++rep.back_open; else if (how.rfind("partial", 0) == 0) ++rep.back_partial;
      if (done) {
        open[i] = 0;
        ++rep.back_decided;
        if (o.verbose) std::cerr << "hsc-pn: backward decided " << properties[i].name << " (" << how << ")\n";
      } else if (o.verbose && !how.empty()) {
        std::cerr << "hsc-pn: backward left " << properties[i].name << " (" << how << ")\n";
      }
    }
  }
  rep.back_s = sec(tb, clock::now());
  std::cerr << "hsc-pn: approx refuted=" << rep.refuted << " of " << rep.open_before << " skipped_removed=" << rep.skipped_removed
            << " test_timeouts=" << s.timeouts() << " tests_s=" << rep.tests_s
            << " back_decided=" << rep.back_decided << " back_init=" << rep.back_init << " back_closed=" << rep.back_closed
            << " back_open=" << rep.back_open << " back_partial=" << rep.back_partial << " back_s=" << rep.back_s
            << " pass_s=" << sec(t0, clock::now()) << '\n';
  return rep;
}

}  // namespace hsc::pn
