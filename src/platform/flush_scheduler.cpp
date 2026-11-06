#include "platform/flush_scheduler.hpp"

#include <algorithm>

namespace as::platform {

void FlushScheduler::request(Clock::time_point now) {
  if (!first_) first_ = now;
  last_ = now;
}

std::optional<FlushScheduler::Clock::time_point> FlushScheduler::dueAt() const {
  if (!first_ || !last_) return std::nullopt;
  return std::min(*last_ + DEBOUNCE, *first_ + MAX_DELAY);
}

bool FlushScheduler::poll(Clock::time_point now) {
  const auto due = dueAt();
  if (!due || now < *due) return false;
  first_.reset();
  last_.reset();
  return true;
}

}  // namespace as::platform
