#include <doctest/doctest.h>

#include <thread>
#include <vector>

#include "hsc/util/timing.hh"

using namespace hsc::util;

TEST_CASE("the stopwatch measures elapsed time monotonically") {
  stopwatch sw;
  const double first = sw.seconds();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  const double second = sw.seconds();
  CHECK(first >= 0.0);
  CHECK(second > first);
  CHECK(second >= 0.004);  // slept 5ms; allow the clock some slack
  CHECK(sw.nanos() > second * 1e9 * 0.5);

  sw.restart();
  CHECK(sw.seconds() < second);
}

TEST_CASE("resident memory is reported and peak does not shrink") {
  const std::size_t peak_before = peak_rss_bytes();
  CHECK(current_rss_bytes() > 0);  // linux; 0 elsewhere would fail loudly
  CHECK(peak_before > 0);

  {
    std::vector<char> ballast(64u << 20, 1);  // 64 MiB, touched
    CHECK(current_rss_bytes() > peak_before / 2);
  }

  // the high water mark survives the release: that is what makes it useful
  CHECK(peak_rss_bytes() >= peak_before);
}
