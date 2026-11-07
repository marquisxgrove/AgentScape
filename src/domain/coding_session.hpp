#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace as::domain {

// coding-sessions Requirement 2: 1 XP per counted minute, a session counts at most 4 hours, and at most 8 hours a
// day (UTC, across all of a user's sessions) earn anything.
inline constexpr int CODING_XP_PER_MINUTE = 1;
inline constexpr std::chrono::seconds CODING_SESSION_MAX{4 * 60 * 60};
inline constexpr std::chrono::seconds CODING_DAILY_CAP{8 * 60 * 60};

using CodingDays = std::map<std::chrono::sys_days, std::chrono::seconds>;   // UTC day -> counted seconds

// Where a session stops counting: `now`, but never past start + CODING_SESSION_MAX nor before `start`.
std::chrono::sys_seconds codingCountedUntil(std::chrono::sys_seconds start, std::chrono::sys_seconds now);

// Credits the interval [from, to) against the daily cap. `used` holds the seconds already counted per UTC day (by
// any session) and is updated in place; the return value is what this call added, per day (days that added nothing
// are left out). The interval is split at UTC midnights, and each part counts only up to what its day has left.
// Crediting [a, b) and then [b, c) adds exactly what crediting [a, c) at once would (Requirement 2.4).
CodingDays creditCodingInterval(CodingDays& used, std::chrono::sys_seconds from, std::chrono::sys_seconds to);

// The XP that `counted` seconds of one session in one day are worth: whole minutes only.
int codingXpFor(std::chrono::seconds counted);

// "YYYY-MM-DD" for a UTC day (the coding_session_day.day column and the XP ledger ref).
std::string formatUtcDay(std::chrono::sys_days day);

// "1h 05m" / "12m" / "0m": a short duration for the Wilds and the town banner.
std::string formatSessionDuration(std::chrono::seconds d);

}  // namespace as::domain
