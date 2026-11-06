#include "core/civil_time.hpp"

#include <array>
#include <cstdio>

namespace as::core {

namespace {
constexpr std::int64_t SECONDS_PER_DAY = 86400;

std::int64_t floorDiv(std::int64_t a, std::int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

std::int64_t daysFromCivil(std::int64_t y, unsigned m, unsigned d) {
  y -= m <= 2 ? 1 : 0;
  const std::int64_t era = floorDiv(y, 400);
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned mp = m > 2 ? m - 3 : m + 9;
  const unsigned doy = (153 * mp + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

bool parseUnsigned(std::string_view s, unsigned& out) {
  out = 0;
  if (s.empty()) return false;
  for (const char c : s) {
    if (c < '0' || c > '9') return false;
    out = out * 10 + static_cast<unsigned>(c - '0');
  }
  return true;
}
}  // namespace

CivilDateTime toCivil(std::chrono::sys_seconds t) {
  const std::int64_t secs = t.time_since_epoch().count();
  const std::int64_t days = floorDiv(secs, SECONDS_PER_DAY);
  const auto sod = static_cast<unsigned>(secs - days * SECONDS_PER_DAY);

  const std::int64_t z = days + 719468;
  const std::int64_t era = floorDiv(z, 146097);
  const auto doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;

  CivilDateTime c;
  c.day = doy - (153 * mp + 2) / 5 + 1;
  c.month = mp < 10 ? mp + 3 : mp - 9;
  c.year = static_cast<std::int64_t>(yoe) + era * 400 + (c.month <= 2 ? 1 : 0);
  c.hour = sod / 3600;
  c.minute = (sod % 3600) / 60;
  c.second = sod % 60;
  return c;
}

std::chrono::sys_seconds fromCivil(const CivilDateTime& c) {
  const std::int64_t days = daysFromCivil(c.year, c.month, c.day);
  return std::chrono::sys_seconds{
      std::chrono::seconds{days * SECONDS_PER_DAY + c.hour * 3600LL + c.minute * 60LL + c.second}};
}

std::string formatIso8601(std::chrono::sys_seconds t) {
  const auto c = toCivil(t);
  std::array<char, 32> buf{};
  std::snprintf(buf.data(), buf.size(), "%04lld-%02u-%02uT%02u:%02u:%02uZ", static_cast<long long>(c.year), c.month,
                c.day, c.hour, c.minute, c.second);
  return buf.data();
}

std::optional<std::chrono::sys_seconds> parseIso8601(std::string_view s) {
  // YYYY-MM-DDTHH:MM:SSZ
  if (s.size() != 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':' || s[19] != 'Z') {
    return std::nullopt;
  }
  unsigned y = 0;
  CivilDateTime c;
  if (!parseUnsigned(s.substr(0, 4), y) || !parseUnsigned(s.substr(5, 2), c.month) ||
      !parseUnsigned(s.substr(8, 2), c.day) || !parseUnsigned(s.substr(11, 2), c.hour) ||
      !parseUnsigned(s.substr(14, 2), c.minute) || !parseUnsigned(s.substr(17, 2), c.second)) {
    return std::nullopt;
  }
  c.year = y;
  if (c.month < 1 || c.month > 12 || c.day < 1 || c.hour > 23 || c.minute > 59 || c.second > 59) return std::nullopt;
  const auto t = fromCivil(c);
  const auto back = toCivil(t);
  if (back.day != c.day || back.month != c.month) return std::nullopt;  // e.g. 2026-02-30
  return t;
}

std::string formatCompactStamp(std::chrono::sys_seconds t) {
  const auto c = toCivil(t);
  std::array<char, 32> buf{};
  std::snprintf(buf.data(), buf.size(), "%04lld%02u%02uT%02u%02u%02uZ", static_cast<long long>(c.year), c.month, c.day,
                c.hour, c.minute, c.second);
  return buf.data();
}

}  // namespace as::core
