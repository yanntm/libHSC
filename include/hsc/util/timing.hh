/// \file timing.hh
/// \brief Wall-clock and resident-memory sampling.
///
/// Kept in the library rather than in bench/ because the invoice reports
/// these alongside the table and cache meters: time and footprint are two of
/// its columns, not an experimental afterthought.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace hsc::util {

/// \brief Monotonic elapsed-time measurement.
class stopwatch {
 public:
  stopwatch() noexcept : start_(clock::now()) {}

  void restart() noexcept { start_ = clock::now(); }

  [[nodiscard]] double seconds() const noexcept {
    return std::chrono::duration<double>(clock::now() - start_).count();
  }

  [[nodiscard]] double nanos() const noexcept {
    return std::chrono::duration<double, std::nano>(clock::now() - start_)
        .count();
  }

 private:
  using clock = std::chrono::steady_clock;
  clock::time_point start_;
};

/// \brief Peak resident set size in bytes, or 0 where unavailable.
///
/// The high-water mark, not the current value: a measurement taken after a
/// collection should still report what the run actually cost.
[[nodiscard]] std::size_t peak_rss_bytes() noexcept;

/// \brief Current resident set size in bytes, or 0 where unavailable.
[[nodiscard]] std::size_t current_rss_bytes() noexcept;

}  // namespace hsc::util
