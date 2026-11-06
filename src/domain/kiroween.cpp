#include "domain/kiroween.hpp"

#include <cctype>
#include <cstdint>

namespace as::domain {

namespace {

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto ca = std::tolower(static_cast<unsigned char>(a[i]));
    const auto cb = std::tolower(static_cast<unsigned char>(b[i]));
    if (ca != cb) {
      return false;
    }
  }
  return true;
}

struct MonthDay {
  unsigned month;
  unsigned day;
};

// Howard Hinnant's civil_from_days; avoids relying on <chrono> calendar support (GCC < 13).
MonthDay utcMonthDay(std::chrono::sys_seconds now) {
  const auto secs = now.time_since_epoch().count();
  std::int64_t days = secs / 86400;
  if (secs % 86400 < 0) {
    --days;
  }
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const auto doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned day = doy - (153 * mp + 2) / 5 + 1;
  const unsigned month = mp < 10 ? mp + 3 : mp - 9;
  return {month, day};
}

}  // namespace

std::optional<KiroweenSetting> parseKiroweenSetting(std::string_view s) {
  if (equalsIgnoreCase(s, "auto")) return KiroweenSetting::Auto;
  if (equalsIgnoreCase(s, "on")) return KiroweenSetting::On;
  if (equalsIgnoreCase(s, "off")) return KiroweenSetting::Off;
  return std::nullopt;
}

std::optional<KiroweenSetting> resolveKiroweenSetting(
    std::span<const std::optional<std::string_view>> sourcesByPrecedence) {
  for (const auto& source : sourcesByPrecedence) {
    if (!source) continue;
    if (auto parsed = parseKiroweenSetting(*source)) {
      return parsed;
    }
  }
  return std::nullopt;
}

bool isKiroweenActive(std::optional<KiroweenSetting> setting, std::chrono::sys_seconds now) {
  if (setting == KiroweenSetting::On) return true;
  if (setting == KiroweenSetting::Off) return false;
  const auto [month, day] = utcMonthDay(now);
  return month == 10 || (month == 11 && day <= 15);
}

}  // namespace as::domain
