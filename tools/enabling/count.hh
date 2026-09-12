#pragma once
#include <gmpxx.h>
#include <map>
#include <algorithm>
#include "hsc/petri/core/Arithmetic.hpp"
#include <ostream>
#include "hsc/petri/reduction/counting/Enabling.h"

namespace hsc::pn {
/// Exact compositions of a residual total, with cooperative interruption.
template<class Check>
mpz_class enabling_ways(long long tokens, std::size_t size, Check& check) {
  if (tokens < 0) return 0;
  mpz_class value = 1;
  const auto k = static_cast<unsigned long long>(size - 1);
  const auto n = static_cast<unsigned long long>(tokens);
  const auto steps = std::min(k, n), base = std::max(k, n);
  for (unsigned long long i = 1; i <= steps; ++i) {
    check();
    value *= mpz_class(std::to_string(base)) + mpz_class(std::to_string(i));
    value /= mpz_class(std::to_string(i));
  }
  return value;
}

/// Original guards over the reduced DD. The callback counts only live groups;
/// fixed groups are included here, never through the solver's PCONST factor.
template<class Query, class Check>
mpz_class count_original_enablings(const ::petri::reduction::Enabling<int>& record,
                                  const std::vector<std::string>& names,
                                  Query query, Check check, std::ostream* trace,
                                  std::ostream* diagnostics) {
  using demand_key = std::vector<std::pair<std::size_t, long long>>;
  std::map<demand_key, mpz_class> cache;
  std::map<std::size_t, std::size_t> live;
  std::map<std::size_t, mpz_class> fixed_ways;
  mpz_class constants = 1;
  if (record.current.size() != names.size()) throw std::logic_error("Enabling coordinates differ from net");
  for (std::size_t p = 0; p < names.size(); ++p) {
    check();
    if (!live.emplace(record.root(record.current[p]), p).second)
      throw std::logic_error("Repeated live enabling group");
  }
  for (std::size_t g = 0; g < record.groups.size(); ++g) {
    check();
    const auto& group = record.groups[g];
    if (group.parent != g) continue;
    if (group.fixed) {
      const auto ways = enabling_ways(*group.fixed, group.size, check);
      constants *= ways;
      fixed_ways.emplace(g, ways);
    } else if (!live.contains(g)) throw std::logic_error("Lost enabling group");
  }
  mpz_class total = 0;
  for (std::size_t t = 0; t < record.origin->names.size(); ++t) {
    check();
    std::map<std::size_t, long long> demands;
    const auto& pre = record.origin->pre.getColumn(t);
    for (std::size_t i = 0; i < pre.size(); ++i) {
      check();
      const auto root = record.root(pre.keyAt(i));
      demands[root] = ::petri::addExact(demands[root], static_cast<long long>(pre.valueAt(i)));
    }
    mpz_class factor = constants;
    demand_key key;
    for (const auto& [g, d] : demands) {
      check();
      const auto& group = record.groups[g];
      if (group.fixed) {
        factor /= fixed_ways.at(g);
        factor *= enabling_ways(static_cast<long long>(*group.fixed) - d, group.size, check);
      } else key.emplace_back(live.at(g), d);
    }
    std::sort(key.begin(), key.end());
    mpz_class enabled = 0;
    if (factor != 0) {
      auto hit = cache.find(key);
      if (hit == cache.end()) {
        std::string form = "(count-enabled R";
        for (const auto& [p, d] : key) {
          check();
          form += " (" + names[p] + " " + std::to_string(d) + ")";
        }
        hit = cache.emplace(key, query(form + ")")).first;
      }
      enabled = hit->second * factor;
    }
    if (trace) *trace << "hsc-pn: enabling " << record.origin->names[t] << ' ' << enabled << '\n';
    total += enabled;
  }
  check();
  if (diagnostics) *diagnostics << "hsc-pn: original enablings transitions=" << record.origin->names.size()
                                << " dd_queries=" << cache.size() << '\n';
  return total;
}
}
