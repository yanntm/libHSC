/// \file surface_reorder.cc
/// \brief Shape passes: `(reorder-force)` — the FORCE heuristic run on
/// the spec's own event supports, emitting a new flat spine — and
/// `(flatten)` — the spine in the current frontier order. Reordering is
/// a rewriting, and semantically neutral: any shape over the same leaves
/// denotes the same set of states.

#include <numeric>
#include <sstream>

#include "hsc/order/force.hh"
#include "hsc/surface/rewrite.hh"
#include "hsc/surface/spec.hh"
#include "hsc/xpl/interpret/model.hh"

namespace hsc::surface {

namespace {

/// Replace the `(shape …)` form by a spine of \p names in order.
std::vector<datum> respine(std::vector<datum> forms,
                           const std::vector<std::string>& names) {
  for (datum& f : forms) {
    if (!f.is_list() || f.items().empty() || f.head() != "shape") continue;
    const int line = f.line();
    std::vector<datum> spine{datum::atom("spine", line)};
    for (const std::string& n : names) spine.push_back(datum::atom(n, line));
    f = datum::list({datum::atom("shape", line),
                     datum::list(std::move(spine), line)},
                    line);
  }
  return forms;
}

}  // namespace

rewrite_result reorder_force(std::vector<datum> forms, const datum&) {
  const spec s = spec::read(forms);
  if (s.order().empty()) return {std::move(forms), false, "no shape"};
  lia::expr_factory ex;
  const expr_reader reader(ex, s);
  const xpl::model m =
      build_xpl_model(s.order().size(), ex, reader, s, s.events());
  // one clique per event: the variables it touches, kept close
  std::vector<order::clique> cliques;
  for (const xpl::event& e : m.events) {
    order::clique c;
    for (std::size_t i = 0; i < e.reads.size(); ++i) {
      c.vars.push_back(static_cast<std::uint32_t>(e.reads.keyAt(i)));
    }
    for (std::size_t i = 0; i < e.writes.size(); ++i) {
      const auto p = static_cast<std::uint32_t>(e.writes.keyAt(i));
      if (!e.reads.get(p)) c.vars.push_back(p);
    }
    if (c.vars.size() >= 2) cliques.push_back(std::move(c));
  }
  const std::vector<std::uint32_t> perm =
      order::force(s.order().size(), cliques, {});
  std::vector<std::uint32_t> identity(perm.size());
  std::iota(identity.begin(), identity.end(), 0);
  if (perm == identity) {
    return {std::move(forms), false,
            "FORCE keeps the current order (" +
                std::to_string(cliques.size()) + " cliques)"};
  }
  std::vector<std::string> names;
  names.reserve(perm.size());
  std::size_t moved = 0;
  for (std::size_t rank = 0; rank < perm.size(); ++rank) {
    names.push_back(s.order()[perm[rank]]);
    moved += perm[rank] != rank;
  }
  std::ostringstream trace;
  trace << "FORCE moved " << moved << " of " << perm.size() << " variables ("
        << cliques.size() << " cliques); the shape is now their spine";
  return {respine(std::move(forms), names), true, trace.str()};
}

rewrite_result flatten(std::vector<datum> forms, const datum&) {
  const spec s = spec::read(forms);
  if (s.order().empty()) return {std::move(forms), false, "no shape"};
  // already flat? a spine of exactly the frontier is the fixed point
  for (const datum& f : forms) {
    if (f.is_list() && !f.items().empty() && f.head() == "shape" &&
        f.items().size() > 1 && f.items()[1].is_list() &&
        f.items()[1].head() == "spine" &&
        f.items()[1].items().size() == s.order().size() + 1) {
      bool flat = true;
      for (std::size_t i = 1; i < f.items()[1].items().size(); ++i) {
        flat &= f.items()[1].items()[i].is_atom();
      }
      if (flat) return {std::move(forms), false, "already a flat spine"};
    }
  }
  return {respine(std::move(forms), s.order()), true,
          "the shape is now the flat spine of " +
              std::to_string(s.order().size()) + " leaves"};
}

}  // namespace hsc::surface
