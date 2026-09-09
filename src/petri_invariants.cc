/// \file petri_invariants.cc
/// \brief Flows through the vendored calculator (`hsc/petri/invariants.hh`).
#include "hsc/petri/invariants.hh"

#include <algorithm>
#include <exception>
#include <iostream>
#include <sstream>

#include "hsc/petri/core/Log.h"
#include "hsc/petri/core/MatrixCol.h"
#include "hsc/petri/invariants/InvariantMiddle.h"

namespace hsc::petri {

namespace {
/// The vendored calculator narrates on std::cout; a tool whose stdout is a
/// protocol cannot have that. Everything it prints goes to a sink while it
/// runs, its log stream included.
struct silenced {
  std::ostringstream sink;
  std::streambuf* saved = std::cout.rdbuf(sink.rdbuf());
  silenced() { ::petri::setLogStream(sink); }
  ~silenced() {
    std::cout.rdbuf(saved);
    ::petri::setLogStream(std::cerr);
  }
};
}  // namespace

std::vector<pflow> pflows(const SparsePetriNet<int>& net, int seconds, bool positive) {
  std::vector<pflow> out;
  const silenced quiet;
  try {
    // The incidence matrix, transitions as columns: post - pre.
    MatrixCol<int> incidence =
        MatrixCol<int>::sumProd(-1, net.getFlowPT(), 1, net.getFlowTP());
    auto [basis, perms] = ::petri::InvariantMiddle<int>::computePInvariantsUntil(
        incidence, positive, seconds, ::petri::EliminationHeuristic());
    (void)perms;  // no compression asked: none
    const std::vector<int>& marks = net.getMarks();
    for (std::size_t c = 0; c < basis.getColumnCount(); ++c) {
      const SparseArray<int>& col = basis.getColumn(c);
      pflow f;
      for (std::size_t i = 0; i < col.size(); ++i) {
        const int p = static_cast<int>(col.keyAt(i));
        const int k = col.valueAt(i);
        if (k == 0) continue;
        f.terms.emplace_back(p, k);
        f.constant += static_cast<long long>(k) * marks[static_cast<std::size_t>(p)];
      }
      if (!f.terms.empty()) out.push_back(std::move(f));
    }
  } catch (const std::exception&) {
    out.clear();  // overflow in the elimination: no invariants rather than wrong ones
  }
  std::ranges::sort(out, {}, [](const pflow& f) { return f.terms.size(); });
  return out;
}

}  // namespace hsc::petri
