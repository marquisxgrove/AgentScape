#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace as::core {

// UTC calendar fields. Implemented with Howard Hinnant's civil-date algorithms so it works
// on compilers without C++20 <chrono> calendar support (GCC < 13).
struct CivilDateTime {
  std::int64_t year = 1970;
  unsigned month = 1;  // 1-12
  unsigned day = 1;    // 1-31
  unsigned hour = 0;
  unsigned minute = 0;
  unsigned second = 0;
};

CivilDateTime toCivil(std::chrono::sys_seconds t);
std::chrono::sys_seconds fromCivil(const CivilDateTime& c);

// "2026-11-06T21:52:31Z": the storage format for every timestamp column.
std::string formatIso8601(std::chrono::sys_seconds t);
// Parses exactly the format produced by formatIso8601.
std::optional<std::chrono::sys_seconds> parseIso8601(std::string_view s);

// "20261106T215231Z": filesystem-safe (no ':'), used for backup file suffixes.
std::string formatCompactStamp(std::chrono::sys_seconds t);

}  // namespace as::core
