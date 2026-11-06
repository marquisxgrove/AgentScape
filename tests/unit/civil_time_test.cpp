#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "core/civil_time.hpp"

using namespace as::core;
using std::chrono::sys_seconds;

namespace {
sys_seconds at(std::int64_t secs) { return sys_seconds{std::chrono::seconds{secs}}; }
}  // namespace

TEST_CASE("timestamps format as ISO-8601 UTC", "[unit][core]") {
  CHECK(formatIso8601(at(0)) == "1970-01-01T00:00:00Z");
  CHECK(formatIso8601(at(1793828551)) == "2026-11-04T21:42:31Z");
  CHECK(formatIso8601(at(951782400)) == "2000-02-29T00:00:00Z");  // leap day
  CHECK(formatIso8601(at(-1)) == "1969-12-31T23:59:59Z");
}

TEST_CASE("ISO-8601 parsing round-trips and rejects malformed input", "[unit][core]") {
  for (const std::int64_t s : {0LL, 951782400LL, 1793828551LL, 4102444799LL}) {
    const auto text = formatIso8601(at(s));
    const auto back = parseIso8601(text);
    REQUIRE(back.has_value());
    CHECK(*back == at(s));
  }
  CHECK_FALSE(parseIso8601("2026-02-30T00:00:00Z").has_value());
  CHECK_FALSE(parseIso8601("2026-13-01T00:00:00Z").has_value());
  CHECK_FALSE(parseIso8601("2026-11-06 21:52:31Z").has_value());
  CHECK_FALSE(parseIso8601("2026-11-06T21:52:31").has_value());
  CHECK_FALSE(parseIso8601("2026-11-06T24:00:00Z").has_value());
  CHECK_FALSE(parseIso8601("").has_value());
}

TEST_CASE("compact stamps are filesystem-safe", "[unit][core]") {
  const auto stamp = formatCompactStamp(at(1793828551));
  CHECK(stamp == "20261104T214231Z");
  CHECK(stamp.find(':') == std::string::npos);
}
