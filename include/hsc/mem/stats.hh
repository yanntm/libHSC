/// \file stats.hh
/// \brief Meters. Nothing is added to mem/ without one.
///
/// The invoice of the calculus is assembled from these. A
/// statistics struct is a plain aggregate that can describe itself as a list
/// of named fields; the printers below turn any such list into either
/// key:value lines (for reading) or a CSV row (for the report artefacts).
/// Descriptions are runtime data, not template machinery, so a new meter costs
/// one `fields()` method and no instantiation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string_view>
#include <vector>

namespace hsc::mem {

/// One named measurement. Counts print as integers, ratios as reals.
struct stat_field {
  const char* name;
  double value;
  bool integral;

  static stat_field count(const char* n, std::uint64_t v) noexcept {
    return {n, static_cast<double>(v), true};
  }
  static stat_field ratio(const char* n, double v) noexcept {
    return {n, v, false};
  }
};

using stat_fields = std::vector<stat_field>;

/// Write `prefix.name: value` lines, one per field.
void print_fields(std::ostream& os, std::string_view prefix,
                  const stat_fields& fields);

/// Write the CSV header for \p fields. \p lead names columns the caller
/// writes itself in front of every row (the experiment's parameters).
void print_csv_header(std::ostream& os, const stat_fields& fields,
                      std::string_view lead = {});

/// Write \p fields as one CSV row, with no leading separator.
void print_csv_row(std::ostream& os, const stat_fields& fields);

/// \brief Meters of a unique table (an intern<T>).
struct intern_statistics {
  std::uint64_t size = 0;         ///< live entries
  std::uint64_t peak = 0;         ///< high water mark of size
  std::uint64_t capacity = 0;     ///< slots in the index table, live or free
  std::uint64_t buckets = 0;      ///< slots in the probe table
  std::uint64_t lookups = 0;      ///< calls to get()
  std::uint64_t hits = 0;         ///< lookups that found an existing entry
  std::uint64_t misses = 0;       ///< lookups that constructed a new entry
  std::uint64_t probes = 0;       ///< slots examined across all lookups
  std::uint64_t rehashes = 0;     ///< probe table rebuilds for growth
  std::uint64_t collections = 0;  ///< garbage collections run
  std::uint64_t collected = 0;    ///< entries freed by collection, cumulative
  std::uint64_t recycled = 0;     ///< ids taken from the free list
  std::uint64_t generation_bumps = 0;  ///< id reuses that invalidated a code

  [[nodiscard]] stat_fields fields() const;
};

/// \brief Meters of an operation cache.
struct cache_statistics {
  std::uint64_t size = 0;       ///< live entries
  std::uint64_t capacity = 0;   ///< entries the cache may hold
  std::uint64_t buckets = 0;    ///< slots in the (never rehashed) table
  std::uint64_t lookups = 0;    ///< operations submitted
  std::uint64_t hits = 0;       ///< answered from the table
  std::uint64_t misses = 0;     ///< evaluated and inserted
  std::uint64_t filtered = 0;   ///< evaluated without being cached
  std::uint64_t evicted = 0;    ///< entries dropped by the eviction policy
  std::uint64_t evictions = 0;  ///< eviction rounds (batches)
  std::uint64_t probes = 0;     ///< slots examined across all lookups

  [[nodiscard]] stat_fields fields() const;
};

}  // namespace hsc::mem
