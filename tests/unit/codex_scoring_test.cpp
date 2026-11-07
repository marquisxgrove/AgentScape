#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <vector>

#include "domain/rate_limit.hpp"
#include "domain/skills.hpp"
#include "domain/slug.hpp"
#include "domain/voting.hpp"

using namespace as::domain;
using namespace std::chrono_literals;

TEST_CASE("known ratings give the documented levels", "[unit][codex]") {
  // Expected values computed independently in Python from the spec formula.
  const auto level = [](std::vector<int> r) { return skillLevel(aggOf(r)); };
  CHECK(level({5, 5, 5}) == 68);
  CHECK(level({1, 1, 1}) == 32);
  CHECK(level({3, 3, 3}) == 50);
  CHECK(level({5, 4, 3}) == 59);
  CHECK(level(std::vector<int>(100, 5)) == 97);  // design: 100 straight 5-star ratings give about 97
  CHECK(level(std::vector<int>(100, 1)) == 3);
  CHECK_FALSE(level({5, 5}).has_value());         // fewer than 3 ratings: unranked
  CHECK_FALSE(level({}).has_value());
}

TEST_CASE("total level counts unranked skills as 1", "[unit][codex]") {
  std::vector<std::optional<int>> levels(10, std::nullopt);
  CHECK(totalLevel(levels) == 10);       // the minimum
  levels[0] = 99;
  levels[5] = 50;
  CHECK(totalLevel(levels) == 99 + 50 + 8);
  CHECK(totalLevel(std::vector<std::optional<int>>(10, 99)) == 990);  // the maximum
}

TEST_CASE("ratings outside 1..5 are clamped, not trusted", "[unit][codex]") {
  CHECK(aggOf(std::vector<int>{0, 9, -4}) == Agg{1 + 5 + 1, 3});
}

TEST_CASE("stat keys and quest types round-trip through their storage names", "[unit][codex]") {
  for (const Skill s : SKILLS) {
    const auto back = statKeyFromStorage(storageName(StatKey{s}));
    REQUIRE(back.has_value());
    CHECK(std::get<Skill>(*back) == s);
  }
  CHECK(storageName(StatKey{Skill::ToolUse}) == "TOOL_USE");
  CHECK(std::holds_alternative<Overall>(*statKeyFromStorage("OVERALL")));
  CHECK_FALSE(statKeyFromStorage("tool_use").has_value());   // exact match only
  CHECK_FALSE(statKeyFromStorage("SPEEDY").has_value());
  for (const QuestType t : QUEST_TYPES) CHECK(questTypeFromStorage(storageName(t)) == t);
  CHECK_FALSE(questTypeFromStorage("NOPE").has_value());
  CHECK(std::string(displayName(Skill::ToolUse)) == "Tool Use");
  CHECK(SKILLS.size() == 10);
  CHECK(QUEST_TYPES.size() == 8);
}

TEST_CASE("a note with 2 votes never outranks one with 200", "[unit][codex]") {
  CHECK(wilsonLowerBound(2, 0) == Catch::Approx(0.3424).margin(0.0005));
  CHECK(wilsonLowerBound(200, 0) == Catch::Approx(0.9812).margin(0.0005));
  CHECK(wilsonLowerBound(8, 2) == Catch::Approx(0.4902).margin(0.0005));
  CHECK(wilsonLowerBound(80, 20) > wilsonLowerBound(8, 2));  // same ratio, more evidence ranks higher
  CHECK(wilsonLowerBound(2, 0) < wilsonLowerBound(200, 10));
  CHECK(wilsonLowerBound(0, 10) == 0.0);
}

TEST_CASE("votes can be cast, changed and retracted, but not on your own note", "[unit][codex]") {
  VoteMap votes;
  votes = *applyVote(votes, {"ann", "n1", VoteValue::Agree}, "zed");
  votes = *applyVote(votes, {"bob", "n1", VoteValue::Disagree}, "zed");
  CHECK(tally(votes) == Tally{1, 1});
  votes = *applyVote(votes, {"bob", "n1", VoteValue::Agree}, "zed");   // changing a vote
  CHECK(tally(votes) == Tally{2, 0});
  votes = *applyVote(votes, {"ann", "n1", std::nullopt}, "zed");        // retracting
  CHECK(tally(votes) == Tally{1, 0});
  const auto self = applyVote(votes, {"zed", "n1", VoteValue::Agree}, "zed");
  REQUIRE_FALSE(self.has_value());
  CHECK(self.error() == VoteError::SelfVote);
  CHECK(tally(votes) == Tally{1, 0});
}

TEST_CASE("slugs are lowercase ASCII with single dashes", "[unit][codex]") {
  CHECK(slugify("GitHub Copilot") == "github-copilot");
  CHECK(slugify("  Claude   3.5 -- Sonnet!  ") == "claude-3-5-sonnet");
  CHECK(slugify("v0") == "v0");
  CHECK(slugify("Héllo Wörld") == "h-llo-w-rld");   // non-ASCII bytes act as separators
  CHECK(slugify("---") == "");
  CHECK(slugify("") == "");
}

TEST_CASE("the rate limit allows N per window and frees up as time passes", "[unit][codex]") {
  using TP = std::chrono::sys_seconds;
  const TP t0{1000s};
  std::vector<TP> history;
  // 3 proposals per rolling 24 hours (Requirement 4.3).
  for (int i = 0; i < 3; ++i) {
    REQUIRE(isAllowed(history, t0 + std::chrono::seconds{i}, 3, 24h));
    history.push_back(t0 + std::chrono::seconds{i});
  }
  CHECK_FALSE(isAllowed(history, t0 + 10h, 3, 24h));
  CHECK_FALSE(isAllowed(history, t0 + 24h - 1s, 3, 24h));   // the first is still inside the window
  CHECK(isAllowed(history, t0 + 24h, 3, 24h));              // exactly 24h later it has aged out
  CHECK_FALSE(isAllowed(history, t0, 0, 24h));              // a limit of 0 forbids everything
}
