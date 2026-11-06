#pragma once

#include <chrono>
#include <optional>

namespace as::platform {

// Decides when to flush the database to persistent storage (IndexedDB on the web).
// Trailing debounce of DEBOUNCE after the latest request, but never later than MAX_DELAY
// after the first pending request, so continuous writes still flush within 1 s (game-shell Req 8.4).
// Plain C++ with an injected clock so it can be unit-tested.
class FlushScheduler {
 public:
  using Clock = std::chrono::steady_clock;
  static constexpr std::chrono::milliseconds DEBOUNCE{250};
  static constexpr std::chrono::milliseconds MAX_DELAY{900};

  void request(Clock::time_point now);
  // Returns true exactly once per pending batch, when the flush is due; clears the pending state.
  bool poll(Clock::time_point now);
  bool pending() const { return first_.has_value(); }
  std::optional<Clock::time_point> dueAt() const;

 private:
  std::optional<Clock::time_point> first_;
  std::optional<Clock::time_point> last_;
};

}  // namespace as::platform
