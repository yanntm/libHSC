/// \file surface_arrays.cc
/// \brief `(simplify-arrays)`: an array none of whose accesses has a
/// dynamic index dissolves — each `(at a K)` becomes its cell's name,
/// the `(array …)` grouping goes. Supports stop being over-approximated
/// to the whole array, so events regain their true locality; and the
/// analyses see the cells one by one. Composes with
/// `simplify-constants`, which grounds index expressions.

#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "hsc/surface/rewrite.hh"
#include "surface_walk.hh"

namespace hsc::surface {

namespace {

bool int_atom(const datum& d, std::int32_t& v) {
  if (!d.is_atom()) return false;
  try {
    std::size_t used = 0;
    v = std::stoi(d.text(), &used);
    return used == d.text().size();
  } catch (...) {
    return false;
  }
}

/// Mark every array that keeps its grouping: a dynamic `(at a E)`, a
/// family marker `(at@ a δ)`, or a static access out of bounds.
void scan(const datum& d,
          const std::map<std::string, std::vector<std::string>>& cells,
          std::set<std::string>& keep) {
  if (!d.is_list() || d.items().empty()) return;
  const std::string& h = d.head();
  if ((h == "at" || h == "at@") && d.items().size() == 3 &&
      d.items()[1].is_atom() && cells.contains(d.items()[1].text())) {
    const std::string& a = d.items()[1].text();
    std::int32_t k = 0;
    if (h == "at@" || !int_atom(d.items()[2], k) || k < 0 ||
        k >= static_cast<std::int32_t>(cells.at(a).size())) {
      keep.insert(a);  // dynamic, certified-family, or out of bounds
    }
    scan(d.items()[2], cells, keep);
    return;
  }
  for (const datum& kid : d.items()) scan(kid, cells, keep);
}

}  // namespace

rewrite_result simplify_arrays(std::vector<datum> forms, const datum&) {
  std::map<std::string, std::vector<std::string>> cells;
  for (const datum& f : forms) {
    if (f.is_list() && !f.items().empty() && f.head() == "array" &&
        f.items().size() > 2) {
      std::vector<std::string> cs;
      for (std::size_t i = 2; i < f.items().size(); ++i) {
        cs.push_back(f.items()[i].text());
      }
      cells[f.items()[1].text()] = std::move(cs);
    }
  }
  if (cells.empty()) return {std::move(forms), false, "no arrays"};
  std::set<std::string> keep;
  for (const datum& f : forms) scan(f, cells, keep);
  std::map<std::string, std::size_t> folded;  // dissolved array → accesses
  for (const auto& [name, cs] : cells) {
    if (!keep.contains(name)) folded.emplace(name, 0);
  }
  if (folded.empty()) {
    return {std::move(forms), false,
            "every array has a dynamic access; none dissolved"};
  }

  // fold the static accesses of the dissolved arrays, everywhere
  std::function<datum(const datum&)> fold = [&](const datum& d) -> datum {
    if (!d.is_list() || d.items().empty()) return d;
    if (d.head() == "at" && d.items().size() == 3 && d.items()[1].is_atom() &&
        folded.contains(d.items()[1].text())) {
      std::int32_t k = 0;
      if (int_atom(d.items()[2], k)) {  // in bounds: scan() kept the rest
        ++folded[d.items()[1].text()];
        return datum::atom(cells.at(d.items()[1].text())[
                               static_cast<std::size_t>(k)],
                           d.line());
      }
    }
    std::vector<datum> kids;
    kids.reserve(d.items().size());
    for (const datum& kid : d.items()) kids.push_back(fold(kid));
    return datum::list(std::move(kids), d.line());
  };
  std::vector<datum> out;
  out.reserve(forms.size());
  for (datum& f : forms) {
    if (f.is_list() && !f.items().empty() && f.head() == "array" &&
        f.items().size() > 1 && folded.contains(f.items()[1].text())) {
      continue;  // the grouping goes; the cells stay, as the leaves they are
    }
    out.push_back(map_exprs(f, fold));
  }
  std::ostringstream trace;
  trace << folded.size() << (folded.size() > 1 ? " arrays" : " array")
        << " dissolved";
  for (const auto& [name, n] : folded) {
    trace << "\n  " << name << ": " << cells.at(name).size() << " cells, "
          << n << " static accesses folded";
  }
  if (!keep.empty()) {
    trace << "\n  kept (dynamic):";
    for (const std::string& k : keep) trace << ' ' << k;
  }
  return {std::move(out), true, trace.str()};
}

}  // namespace hsc::surface
