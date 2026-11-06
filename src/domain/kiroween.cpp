#include "domain/kiroween.hpp"

#include <cctype>

#include "core/civil_time.hpp"

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
  const auto c = core::toCivil(now);
  return c.month == 10 || (c.month == 11 && c.day <= 15);
}

}  // namespace as::domain
