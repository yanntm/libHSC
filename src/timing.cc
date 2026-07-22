#include "hsc/util/timing.hh"

#include <cstdio>
#include <cstring>

namespace hsc::util {

namespace {

#if defined(__linux__)
/// Read one `VmXXX:` line of /proc/self/status, in bytes. 0 if not found.
std::size_t read_status_field(const char* field) noexcept {
  std::FILE* f = std::fopen("/proc/self/status", "r");
  if (f == nullptr) return 0;
  const std::size_t len = std::strlen(field);
  char line[256];
  std::size_t result = 0;
  while (std::fgets(line, sizeof(line), f) != nullptr) {
    if (std::strncmp(line, field, len) == 0) {
      unsigned long kb = 0;
      if (std::sscanf(line + len, " %lu", &kb) == 1) result = kb * 1024;
      break;
    }
  }
  std::fclose(f);
  return result;
}
#endif

}  // namespace

std::size_t peak_rss_bytes() noexcept {
#if defined(__linux__)
  return read_status_field("VmHWM:");
#else
  return 0;
#endif
}

std::size_t current_rss_bytes() noexcept {
#if defined(__linux__)
  return read_status_field("VmRSS:");
#else
  return 0;
#endif
}

}  // namespace hsc::util
