#pragma once

#include <chrono>
#include <optional>
#include <span>
#include <string_view>

namespace as::domain {

enum class KiroweenSetting { Auto, On, Off };

// Accepts "auto" | "on" | "off" (case-insensitive). Anything else is rejected.
std::optional<KiroweenSetting> parseKiroweenSetting(std::string_view s);

// Sources are ordered by precedence (CLI, env, URL query, build default).
// Returns the first source that is present AND parses.
std::optional<KiroweenSetting> resolveKiroweenSetting(
    std::span<const std::optional<std::string_view>> sourcesByPrecedence);

// On/Off force the result. Auto (or no setting) is active 1 Oct - 15 Nov inclusive, UTC.
bool isKiroweenActive(std::optional<KiroweenSetting> setting, std::chrono::sys_seconds now);

}  // namespace as::domain
