#pragma once

#include <chrono>
#include <span>

namespace as::domain {

// Sliding-window limiter core. `history` holds the times of previously RECORDED attempts. An attempt at
// `now` is allowed when fewer than `limit` recorded attempts fall in the window (now - window, now].
// The caller records an attempt only when this returns true, so no window ever holds more than `limit`.
bool isAllowed(std::span<const std::chrono::sys_seconds> history, std::chrono::sys_seconds now, int limit,
               std::chrono::seconds window);

}  // namespace as::domain
