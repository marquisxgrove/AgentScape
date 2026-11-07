#pragma once

#include <cstdint>

#include "app/event_bus.hpp"

namespace as::services {

// Invalidation signal for memoised Codex views (list pages, comparisons). Anything that changes ratings,
// Agents or Notes publishes an event after it commits; this bumps a generation counter, and a cached view
// is valid only while its recorded generation still matches (design: CodexCache).
class CodexCache {
 public:
  explicit CodexCache(app::EventBus& bus);
  std::uint64_t generation() const { return generation_; }
  void invalidate() { ++generation_; }

 private:
  std::uint64_t generation_ = 1;
  app::Subscription ratings_;
  app::Subscription agents_;
  app::Subscription notes_;
};

}  // namespace as::services
