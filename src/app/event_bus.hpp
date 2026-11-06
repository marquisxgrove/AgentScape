#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "domain/chat_log.hpp"

namespace as::app {

// --- Typed events (published on the main thread, after the publishing service commits) ---
struct ActivityPosted {
  domain::ChatMessage message;
};
struct RatingsChanged {
  std::string agentId;
};
struct XpChanged {
  std::string userId;
  long long totalXp = 0;
  int oldLevel = 1;
  int newLevel = 1;
};
struct ItemUnlocked {
  std::string userId;
  std::string itemId;
};
struct GameMessage {
  std::string text;
};

using Event = std::variant<ActivityPosted, RatingsChanged, XpChanged, ItemUnlocked, GameMessage>;

// Synchronous publish/subscribe. Handlers subscribed during a publish are not called for that event;
// handlers unsubscribed during a publish are skipped.
class EventBus {
 public:
  using SubscriptionId = std::size_t;

  template <typename E>
  SubscriptionId subscribe(std::function<void(const E&)> handler) {
    const SubscriptionId id = nextId_++;
    handlers_.push_back({id, [h = std::move(handler)](const Event& ev) {
                           if (const auto* e = std::get_if<E>(&ev)) h(*e);
                         }});
    return id;
  }

  void unsubscribe(SubscriptionId id);
  void publish(const Event& event);
  std::size_t subscriberCount() const;

 private:
  struct Entry {
    SubscriptionId id;
    std::function<void(const Event&)> fn;
    bool active = true;
  };
  std::vector<Entry> handlers_;
  SubscriptionId nextId_ = 1;
  int publishDepth_ = 0;
  void compact();
};

// RAII subscription that unsubscribes on destruction (screens and widgets hold these).
class Subscription {
 public:
  Subscription() = default;
  Subscription(EventBus& bus, EventBus::SubscriptionId id) : bus_(&bus), id_(id) {}
  ~Subscription() { reset(); }
  Subscription(const Subscription&) = delete;
  Subscription& operator=(const Subscription&) = delete;
  Subscription(Subscription&& o) noexcept : bus_(std::exchange(o.bus_, nullptr)), id_(o.id_) {}
  Subscription& operator=(Subscription&& o) noexcept {
    if (this != &o) {
      reset();
      bus_ = std::exchange(o.bus_, nullptr);
      id_ = o.id_;
    }
    return *this;
  }
  void reset() {
    if (bus_ != nullptr) bus_->unsubscribe(id_);
    bus_ = nullptr;
  }

 private:
  EventBus* bus_ = nullptr;
  EventBus::SubscriptionId id_ = 0;
};

}  // namespace as::app
