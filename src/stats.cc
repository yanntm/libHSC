#include "hsc/mem/stats.hh"

#include <ostream>

namespace hsc::mem {

namespace {

void write_value(std::ostream& os, const stat_field& f) {
  if (f.integral) {
    os << static_cast<std::uint64_t>(f.value);
  } else {
    const auto flags = os.flags();
    const auto prec = os.precision(4);
    os << f.value;
    os.precision(prec);
    os.flags(flags);
  }
}

}  // namespace

void print_fields(std::ostream& os, std::string_view prefix,
                  const stat_fields& fields) {
  for (const auto& f : fields) {
    if (!prefix.empty()) os << prefix << '.';
    os << f.name << ": ";
    write_value(os, f);
    os << '\n';
  }
}

void print_csv_header(std::ostream& os, const stat_fields& fields,
                      std::string_view lead) {
  if (!lead.empty()) os << lead << ',';
  bool first = true;
  for (const auto& f : fields) {
    if (!first) os << ',';
    first = false;
    os << f.name;
  }
  os << '\n';
}

void print_csv_row(std::ostream& os, const stat_fields& fields) {
  bool first = true;
  for (const auto& f : fields) {
    if (!first) os << ',';
    first = false;
    write_value(os, f);
  }
  os << '\n';
}

stat_fields intern_statistics::fields() const {
  const double load = buckets ? static_cast<double>(size) /
                                    static_cast<double>(buckets)
                              : 0.0;
  const double per_lookup = lookups ? static_cast<double>(probes) /
                                          static_cast<double>(lookups)
                                    : 0.0;
  const double hit_rate =
      lookups ? static_cast<double>(hits) / static_cast<double>(lookups) : 0.0;
  return {
      stat_field::count("size", size),
      stat_field::count("peak", peak),
      stat_field::count("capacity", capacity),
      stat_field::count("buckets", buckets),
      stat_field::count("lookups", lookups),
      stat_field::count("hits", hits),
      stat_field::count("misses", misses),
      stat_field::ratio("hit_rate", hit_rate),
      stat_field::count("probes", probes),
      stat_field::ratio("probes_per_lookup", per_lookup),
      stat_field::ratio("load_factor", load),
      stat_field::count("rehashes", rehashes),
      stat_field::count("collections", collections),
      stat_field::count("collected", collected),
      stat_field::count("recycled", recycled),
      stat_field::count("generation_bumps", generation_bumps),
  };
}

stat_fields cache_statistics::fields() const {
  const double hit_rate =
      lookups ? static_cast<double>(hits) / static_cast<double>(lookups) : 0.0;
  const double per_lookup = lookups ? static_cast<double>(probes) /
                                          static_cast<double>(lookups)
                                    : 0.0;
  const double load = buckets ? static_cast<double>(size) /
                                    static_cast<double>(buckets)
                              : 0.0;
  return {
      stat_field::count("size", size),
      stat_field::count("capacity", capacity),
      stat_field::count("buckets", buckets),
      stat_field::count("lookups", lookups),
      stat_field::count("hits", hits),
      stat_field::count("misses", misses),
      stat_field::ratio("hit_rate", hit_rate),
      stat_field::count("filtered", filtered),
      stat_field::count("evicted", evicted),
      stat_field::count("evictions", evictions),
      stat_field::count("probes", probes),
      stat_field::ratio("probes_per_lookup", per_lookup),
      stat_field::ratio("load_factor", load),
  };
}

}  // namespace hsc::mem
