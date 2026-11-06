#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "domain/chat_log.hpp"

using namespace as::domain;

// Shrunk counterexample from Property 4: equal timestamps + trimming must stay idempotent.
TEST_CASE("appending the same batch twice never resurrects a trimmed message", "[unit][regression]") {
  const std::chrono::sys_seconds t{std::chrono::seconds{1}};
  const std::vector<ChatMessage> batch{{"m0", t, Channel::Game, "a"}, {"m1", t, Channel::Game, "b"}};
  const auto once = appendMessages({}, batch, 1);
  const auto twice = appendMessages(once, batch, 1);
  CHECK(once == twice);
  REQUIRE(once.size() == 1);
}
