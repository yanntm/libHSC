/// Optional bound consumer of the projected invariant set. No reachable-set
/// enumeration: an upper bound is exact only if the initial marking attains it.
#pragma once
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include "hsc/petri/core/Arithmetic.hpp"
#include "pn_solver.hh"

namespace hsc::pn {

inline bool bound_from_approx(solver& s, const ::petri::expr::Property& property,
                              const SparsePetriNet<int>& original, double seconds,
                              std::ostream& out) {
  if (seconds <= 0) return false;
  s.set_deadline(std::chrono::steady_clock::now() +
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds)));
  bool decided = false;
  try {
    long long lower = 0;
    std::string query = "(max-sum S";
    for (const auto& [p, c] : property.boundForm().terms) {
      lower = ::petri::addExact(lower, ::petri::multiplyExact(c, static_cast<long long>(original.getMarks()[p])));
      query += " (* " + std::to_string(c) + ' ' + original.getPnames()[p] + ')';
    }
    for (const auto& line : s.feed(query + ')')) {
      const std::string prefix = "S max-sum ";
      if (line.rfind(prefix, 0) != 0) continue;
      const auto value = line.substr(prefix.size());
      if (value == "none") break;
      const long long upper = std::stoll(value);
      if (upper < lower) throw std::runtime_error("approximation bound excludes the initial value");
      std::cerr << "hsc-pn: approx bound " << property.name << " lower=" << lower << " upper=" << upper << '\n';
      if (lower == upper) {
        out << "FORMULA " << property.name << ' ' << upper << APPROX << std::endl;
        decided = true;
      }
      break;
    }
  } catch (const std::exception& e) {
    std::cerr << "hsc-pn: approx bound " << property.name << " unavailable: " << e.what() << '\n';
  }
  s.set_deadline(std::nullopt);
  return decided;
}

}  // namespace hsc::pn
