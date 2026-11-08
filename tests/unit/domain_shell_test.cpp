#include <catch2/catch_test_macros.hpp>

#include <set>

#include "core/ids.hpp"
#include "core/rng.hpp"
#include "domain/guest_name.hpp"
#include "domain/settings.hpp"

using namespace as;

TEST_CASE("settings round-trip and tolerate malformed JSON", "[unit][settings]") {
  domain::Settings s;
  s.plainFont = true;
  s.sound = true;
  CHECK(domain::settingsFromJson(domain::settingsToJson(s)) == s);
  s.fullscreen = false;
  CHECK(domain::settingsFromJson(domain::settingsToJson(s)) == s);
  // Fullscreen is on by default, including for saves from before the setting existed.
  CHECK(domain::Settings{}.fullscreen);
  CHECK(domain::settingsFromJson(R"({"plainFont":false,"reduceMotion":false,"sound":false})").fullscreen);
  // Saves from when it was "Pixel-perfect scaling" keep their choice; the new key wins when both are present.
  CHECK_FALSE(domain::settingsFromJson(R"({"pixelPerfect":true})").fullscreen);
  CHECK(domain::settingsFromJson(R"({"pixelPerfect":false})").fullscreen);
  CHECK(domain::settingsFromJson(R"({"pixelPerfect":true,"fullscreen":true})").fullscreen);

  domain::Settings defaults;
  defaults.reduceMotion = true;
  CHECK(domain::settingsFromJson("not json", defaults) == defaults);
  CHECK(domain::settingsFromJson("{}", defaults) == defaults);
  // Known keys override the default field-by-field; unknown keys and wrong types are ignored.
  const auto partial = domain::settingsFromJson(R"({"plainFont":true,"sound":"yes","x":1})", defaults);
  CHECK(partial.plainFont);
  CHECK(partial.reduceMotion);  // from defaults
  CHECK_FALSE(partial.sound);   // wrong type -> default (false)
}

TEST_CASE("guest names use the safe alphabet and validate", "[unit][guest]") {
  core::SplitMix64 rng(1);
  for (int i = 0; i < 100; ++i) {
    const auto name = domain::randomGuestName(rng);
    CHECK(domain::isGuestName(name));
    CHECK(name.find_first_of("O0I1") == std::string::npos);
  }
  CHECK_FALSE(domain::isGuestName("Guest-"));
  CHECK_FALSE(domain::isGuestName("Guest-abcd"));  // lowercase not in alphabet
  CHECK_FALSE(domain::isGuestName("Player-ABCD"));
}

TEST_CASE("ULIDs are 26 Crockford chars, unique, and time-ordered", "[unit][ids]") {
  core::SplitMix64 rng(7);
  std::set<std::string> seen;
  auto t1 = std::chrono::sys_seconds{std::chrono::seconds{1793800000}};
  for (int i = 0; i < 1000; ++i) {
    const auto id = core::newUlid(t1, rng);
    CHECK(core::isUlid(id));
    CHECK(seen.insert(id).second);
  }
  const auto earlier = core::newUlid(t1, rng);
  const auto later = core::newUlid(t1 + std::chrono::hours{1}, rng);
  CHECK(earlier < later);  // the timestamp prefix orders them
  CHECK_FALSE(core::isUlid("seed:npc:mossbeard"));
  CHECK_FALSE(core::isUlid("short"));
}

TEST_CASE("Rng::below is unbiased across the whole bound", "[unit][rng]") {
  core::SplitMix64 rng(123);
  std::set<std::uint64_t> values;
  for (int i = 0; i < 5000; ++i) {
    const auto v = rng.below(6);
    CHECK(v < 6);
    values.insert(v);
  }
  CHECK(values.size() == 6);  // every face of a d6 appears
}
