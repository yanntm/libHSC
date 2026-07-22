#include <doctest/doctest.h>

#include <thread>

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
