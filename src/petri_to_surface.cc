/// \file petri_to_surface.cc
/// \brief M2T: a P/T net + NUPN unit tree serialised as `.hsc` surface text.

#include "hsc/petri/to_surface.hh"

#include <algorithm>
#include <map>
#include <ostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "hsc/petri/MatrixCol.h"
#include "hsc/petri/nupn.hh"

namespace hsc::petri {

namespace {

/// The shape of a unit, as a sort expression: its places (leaves) and its
/// subunits (nested sorts), bracketed as a spine. A single child collapses to
/// itself so the tree is not padded with trivial pairs. Empty units vanish.
std::string sort_of(const std::string& uid, const unit_tree& t,
                    const std::vector<std::string>& extra_root, bool is_root) {
  const auto it = t.units.find(uid);
  if (it == t.units.end()) return "";
  const unit& u = it->second;

  std::vector<std::string> kids;
  for (const std::string& p : u.places) kids.push_back(p);
  for (const std::string& s : u.subunits) {
    std::string c = sort_of(s, t, extra_root, false);
    if (!c.empty()) kids.push_back(std::move(c));
  }
  if (is_root) {  // orphan places (in no unit) ride under the root
    for (const std::string& p : extra_root) kids.push_back(p);
  }

  if (kids.empty()) return "";
  if (kids.size() == 1) return kids.front();
  std::string out = "(spine";
  for (const std::string& k : kids) {
    out += ' ';
    out += k;
  }
  out += ')';
  return out;
}

/// Places declared but placed in no unit, in declaration order.
std::vector<std::string> orphans(const SparsePetriNet<int>& net,
                                 const unit_tree& units) {
  std::unordered_set<std::string> covered;
  for (const auto& [id, u] : units.units) {
    for (const std::string& p : u.places) covered.insert(p);
  }
  std::vector<std::string> out;
  for (const std::string& p : net.getPnames()) {
    if (!covered.contains(p)) out.push_back(p);
  }
  return out;
}

}  // namespace

void to_surface(std::ostream& out, const SparsePetriNet<int>& net,
                const unit_tree& units, const emit_options& opts) {
  const std::vector<std::string>& pnames = net.getPnames();
  const std::vector<std::string>& tnames = net.getTnames();
  const std::vector<int>& marks = net.getMarks();

  int bound = opts.bound;
  for (int m : marks) bound = std::max(bound, m + 1);

  out << "; Imported from PNML/NUPN by tools/nupn2hsc.\n";
  out << "; net " << net.getName() << ": " << pnames.size() << " places, "
      << tnames.size() << " transitions, safe=" << (units.safe ? "true" : "false")
      << ".\n";

  for (const std::string& p : pnames) {
    out << "(leaf " << p << " 0 " << bound << ")\n";
  }

  const std::vector<std::string> extra = units.present()
                                             ? orphans(net, units)
                                             : std::vector<std::string>{};
  if (units.present()) {
    if (!extra.empty()) {
      out << "; note: " << extra.size()
          << " place(s) outside every unit, placed under the root.\n";
    }
    out << "(shape " << sort_of(units.root, units, extra, true) << ")\n";
  } else {
    out << "; no nupn unit tree: flat spine over all places.\n(shape (spine";
    for (const std::string& p : pnames) out << ' ' << p;
    out << "))\n";
  }

  out << "(init";
  for (std::size_t i = 0; i < pnames.size(); ++i) {
    if (marks[i] != 0) out << " (" << pnames[i] << ' ' << marks[i] << ')';
  }
  out << ")\n";

  const MatrixCol<int>& pre = net.getFlowPT();   // place -> transition (inputs)
  const MatrixCol<int>& post = net.getFlowTP();  // transition -> place (outputs)
  for (std::size_t t = 0; t < tnames.size(); ++t) {
    const SparseArray<int>& in = pre.getColumn(t);
    const SparseArray<int>& to = post.getColumn(t);

    // net per-place effect: outputs add, inputs consume
    std::map<std::size_t, int> delta;
    for (std::size_t k = 0; k < in.size(); ++k) delta[in.keyAt(k)] -= in.valueAt(k);
    for (std::size_t k = 0; k < to.size(); ++k) delta[to.keyAt(k)] += to.valueAt(k);

    if (in.size() == 0 && delta.empty()) continue;  // no-op transition

    out << "(event " << tnames[t] << " (when";
    for (std::size_t k = 0; k < in.size(); ++k) {
      out << " (>= " << pnames[in.keyAt(k)] << ' ' << in.valueAt(k) << ')';
    }
    out << ") (do";
    for (const auto& [p, d] : delta) {
      if (d > 0) out << " (+= " << pnames[p] << ' ' << d << ')';
      else if (d < 0) out << " (-= " << pnames[p] << ' ' << -d << ')';
    }
    out << "))\n";
  }

  switch (opts.exam) {
    case examination::state_space: out << "(states)\n"; break;
    case examination::one_safe:    out << "(one-safe)\n"; break;
    case examination::deadlock:    out << "(deadlock)\n"; break;
    case examination::none:
      out << "(reach R saturate)\n(count R)\n(nodes R)\n(bill)\n";
      break;
  }
}

}  // namespace hsc::petri
