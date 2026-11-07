#include "domain/rate_limit.hpp"

namespace as::domain {

bool isAllowed(std::span<const std::chrono::sys_seconds> history, std::chrono::sys_seconds now, int limit,
               std::chrono::seconds window) {
  if (limit <= 0) return false;
  int inWindow = 0;
  for (const auto& t : history) {
    if (t <= now && now - t < window) ++inWindow;
  }
  return inWindow < limit;
}

}  // namespace as::domain
