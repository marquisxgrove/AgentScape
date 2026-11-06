#include <catch2/catch_test_macros.hpp>

#include "platform/flush_scheduler.hpp"

using as::platform::FlushScheduler;
using namespace std::chrono_literals;

namespace {
const FlushScheduler::Clock::time_point T0{};
}

TEST_CASE("nothing flushes without a request", "[unit][persist]") {
  FlushScheduler s;
  CHECK_FALSE(s.poll(T0 + 10s));
  CHECK_FALSE(s.pending());
}

TEST_CASE("a single commit flushes 250 ms later, exactly once", "[unit][persist]") {
  FlushScheduler s;
  s.request(T0);
  CHECK_FALSE(s.poll(T0 + 249ms));
  CHECK(s.poll(T0 + 250ms));
  CHECK_FALSE(s.poll(T0 + 400ms));
}

TEST_CASE("bursts of commits are coalesced into one flush", "[unit][persist]") {
  FlushScheduler s;
  s.request(T0);
  s.request(T0 + 100ms);
  s.request(T0 + 200ms);
  CHECK_FALSE(s.poll(T0 + 300ms));
  CHECK(s.poll(T0 + 450ms));
}

TEST_CASE("continuous commits still flush within 1 s of the first", "[unit][persist]") {
  FlushScheduler s;
  bool flushed = false;
  auto now = T0;
  for (int i = 0; i < 50 && !flushed; ++i, now += 100ms) {
    s.request(now);
    flushed = s.poll(now);
  }
  REQUIRE(flushed);
  CHECK(now - T0 <= 1s);
}
