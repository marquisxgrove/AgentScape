#include "app/event_bus.hpp"

#include <algorithm>

namespace as::app {

void EventBus::unsubscribe(SubscriptionId id) {
  for (auto& e : handlers_) {
    if (e.id == id) e.active = false;
  }
  compact();
}

void EventBus::publish(const Event& event) {
  ++publishDepth_;
  // Index loop with a fixed bound: handlers added during dispatch are not invoked for this event,
  // and push_back reallocation can't invalidate what we're iterating.
  const std::size_t count = handlers_.size();
  for (std::size_t i = 0; i < count; ++i) {
    if (handlers_[i].active) {
      auto fn = handlers_[i].fn;  // copy: the handler may (un)subscribe and reallocate the vector
      fn(event);
    }
  }
  --publishDepth_;
  compact();
}

std::size_t EventBus::subscriberCount() const {
  return static_cast<std::size_t>(
      std::count_if(handlers_.begin(), handlers_.end(), [](const Entry& e) { return e.active; }));
}

void EventBus::compact() {
  if (publishDepth_ > 0) return;
  handlers_.erase(std::remove_if(handlers_.begin(), handlers_.end(), [](const Entry& e) { return !e.active; }),
                  handlers_.end());
}

}  // namespace as::app
