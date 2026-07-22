#include <doctest/doctest.h>

#include <sstream>
#include <string>

#include "hsc/mem/stats.hh"

using namespace hsc::mem;

TEST_CASE("meters describe themselves as named fields") {
  intern_statistics s;
  s.lookups = 10;
  s.hits = 7;
  s.misses = 3;
  s.size = 3;
  s.buckets = 8;

  const auto fields = s.fields();
  bool saw_hit_rate = false;
  for (const auto& f : fields) {
    if (std::string(f.name) == "hit_rate") {
      saw_hit_rate = true;
      CHECK_FALSE(f.integral);
      CHECK(f.value == doctest::Approx(0.7));
    }
  }
  CHECK(saw_hit_rate);
}

TEST_CASE("key:value output is one line per field") {
  cache_statistics s;
  s.lookups = 4;
  s.hits = 1;
  std::ostringstream os;
  print_fields(os, "memo", s.fields());
  const std::string out = os.str();
  CHECK(out.find("memo.lookups: 4\n") != std::string::npos);
  CHECK(out.find("memo.hits: 1\n") != std::string::npos);
  CHECK(out.find("memo.hit_rate: 0.25\n") != std::string::npos);
}

TEST_CASE("CSV header and rows line up") {
  intern_statistics s;
  s.size = 2;
  const auto fields = s.fields();

  std::ostringstream header, row;
  print_csv_header(header, fields, "run,n");
  print_csv_row(row, fields);

  const auto count = [](const std::string& line) {
    return std::count(line.begin(), line.end(), ',');
  };
  // the header carries two extra lead columns the caller writes itself
  CHECK(count(header.str()) == count(row.str()) + 2);
  CHECK(header.str().starts_with("run,n,size,"));
  CHECK(row.str().starts_with("2,"));
}
