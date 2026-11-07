#include "domain/coding_session.hpp"

#include <algorithm>
#include <cstdio>

#include "core/civil_time.hpp"

namespace as::domain {

using std::chrono::seconds;
using std::chrono::sys_days;
using std::chrono::sys_seconds;

sys_seconds codingCountedUntil(sys_seconds start, sys_seconds now) {
  return std::clamp(now, start, start + CODING_SESSION_MAX);
}

CodingDays creditCodingInterval(CodingDays& used, sys_seconds from, sys_seconds to) {
  CodingDays added;
  while (from < to) {
    const sys_days day = std::chrono::floor<std::chrono::days>(from);
    const sys_seconds dayEnd = sys_seconds{day + std::chrono::days{1}};
    const sys_seconds partEnd = std::min(to, dayEnd);
    seconds& usedToday = used[day];
    const seconds room = std::max(seconds{0}, CODING_DAILY_CAP - usedToday);
    const seconds add = std::min(partEnd - from, room);
    if (add > seconds{0}) {
      usedToday += add;
      added[day] += add;
    }
    if (usedToday == seconds{0}) used.erase(day);   // keep `used` free of empty days, so maps compare cleanly
    from = partEnd;
  }
  return added;
}

int codingXpFor(seconds counted) {
  if (counted <= seconds{0}) return 0;
  return static_cast<int>(counted.count() / 60) * CODING_XP_PER_MINUTE;
}

std::string formatUtcDay(sys_days day) { return core::formatIso8601(sys_seconds{day}).substr(0, 10); }

std::string formatSessionDuration(seconds d) {
  const long long total = std::max<long long>(0, d.count());
  const long long hours = total / 3600;
  const long long minutes = (total % 3600) / 60;
  char buf[32];
  if (hours > 0) {
    std::snprintf(buf, sizeof buf, "%lldh %02lldm", hours, minutes);
  } else {
    std::snprintf(buf, sizeof buf, "%lldm", minutes);
  }
  return buf;
}

}  // namespace as::domain
