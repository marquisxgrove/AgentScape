#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "app/event_bus.hpp"

using namespace as::app;

TEST_CASE("subscribers receive only the event type they subscribed to", "[unit][event-bus]") {
  EventBus bus;
  std::vector<std::string> got;
  bus.subscribe<GameMessage>([&](const GameMessage& m) { got.push_back(m.text); });
  bus.publish(GameMessage{"Congratulations, you just advanced a level!"});
  bus.publish(RatingsChanged{"agent-1"});
  REQUIRE(got == std::vector<std::string>{"Congratulations, you just advanced a level!"});
}

TEST_CASE("handlers run in subscription order", "[unit][event-bus]") {
  EventBus bus;
  std::string order;
  bus.subscribe<GameMessage>([&](const GameMessage&) { order += "a"; });
  bus.subscribe<GameMessage>([&](const GameMessage&) { order += "b"; });
  bus.publish(GameMessage{"x"});
  CHECK(order == "ab");
}

TEST_CASE("a Subscription unsubscribes when it goes out of scope", "[unit][event-bus]") {
  EventBus bus;
  int calls = 0;
  {
    Subscription sub{bus, bus.subscribe<GameMessage>([&](const GameMessage&) { ++calls; })};
    bus.publish(GameMessage{"x"});
  }
  bus.publish(GameMessage{"y"});
  CHECK(calls == 1);
  CHECK(bus.subscriberCount() == 0);
}

TEST_CASE("subscribing during publish does not deliver the in-flight event", "[unit][event-bus]") {
  EventBus bus;
  int late = 0;
  bus.subscribe<GameMessage>([&](const GameMessage&) {
    bus.subscribe<GameMessage>([&](const GameMessage&) { ++late; });
  });
  bus.publish(GameMessage{"first"});
  CHECK(late == 0);
  bus.publish(GameMessage{"second"});
  CHECK(late == 1);
}

TEST_CASE("unsubscribing during publish skips the removed handler", "[unit][event-bus]") {
  EventBus bus;
  int second = 0;
  EventBus::SubscriptionId secondId = 0;
  bus.subscribe<GameMessage>([&](const GameMessage&) { bus.unsubscribe(secondId); });
  secondId = bus.subscribe<GameMessage>([&](const GameMessage&) { ++second; });
  bus.publish(GameMessage{"x"});
  CHECK(second == 0);
  CHECK(bus.subscriberCount() == 1);
}
