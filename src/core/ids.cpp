#include "core/ids.hpp"

#include <array>
#include <cstdint>

namespace as::core {

namespace {
constexpr std::string_view CROCKFORD = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr std::size_t ULID_LEN = 26;
}  // namespace

std::string newUlid(std::chrono::sys_seconds now, Rng& rng) {
  auto ms = static_cast<std::uint64_t>(now.time_since_epoch().count()) * 1000ULL;
  std::string out(ULID_LEN, '0');
  for (int i = 9; i >= 0; --i) {  // 10 chars = 50 bits (top 2 bits are always 0 for 48-bit time)
    out[static_cast<std::size_t>(i)] = CROCKFORD[ms & 31U];
    ms >>= 5;
  }
  // 80 random bits = 16 chars; take 5 bits at a time from fresh 64-bit draws.
  std::uint64_t pool = 0;
  int bits = 0;
  for (std::size_t i = 10; i < ULID_LEN; ++i) {
    if (bits < 5) {
      pool = rng.nextU64();
      bits = 64;
    }
    out[i] = CROCKFORD[pool & 31U];
    pool >>= 5;
    bits -= 5;
  }
  return out;
}

bool isUlid(std::string_view s) {
  if (s.size() != ULID_LEN) return false;
  if (s.front() > '7') return false;  // 128-bit range: first char encodes only 3 bits
  for (const char c : s) {
    if (CROCKFORD.find(c) == std::string_view::npos) return false;
  }
  return true;
}

}  // namespace as::core
