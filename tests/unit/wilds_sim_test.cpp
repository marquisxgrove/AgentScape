#include <catch2/catch_test_macros.hpp>

#include <numeric>

#include "game/wilds_sim.hpp"

using namespace as::game;

TEST_CASE("every fight's hits add up to the bug's health exactly", "[unit][coding-sessions]") {
  for (std::uint64_t seed : {1ULL, 42ULL, wildsSeed("01JABCDEF")}) {
    for (std::int64_t fight = 0; fight < 200; ++fight) {
      const auto d = fightDamage(seed, fight);
      const auto hp = combatAt(seed, static_cast<double>(fight) * FIGHT_SECONDS, false).hpMax;
      CHECK(std::accumulate(d.begin(), d.end(), 0) == hp);
      for (const int hit : d) CHECK(hit >= 1);
    }
  }
}

TEST_CASE("a fight runs from full health to squashed, and the tally counts each squash once", "[unit][coding-sessions]") {
  const auto seed = wildsSeed("session-1");
  const auto start = combatAt(seed, 0.0, true);
  CHECK(start.hpLeft == start.hpMax);
  CHECK_FALSE(start.lastHit.has_value());
  CHECK(start.squashedTotal == 0);

  const auto first = combatAt(seed, HIT_SECONDS + 0.1, true);
  REQUIRE(first.lastHit.has_value());
  CHECK_FALSE(first.lastHit->byFamiliar);      // the Adventurer opens
  const auto second = combatAt(seed, 2 * HIT_SECONDS + 0.1, true);
  CHECK(second.lastHit->byFamiliar);           // the familiar lands every other hit
  CHECK_FALSE(combatAt(seed, 2 * HIT_SECONDS + 0.1, false).lastHit->byFamiliar);   // solo: never

  const auto dead = combatAt(seed, HITS_PER_FIGHT * HIT_SECONDS + 0.1, true);
  CHECK(dead.squashed);
  CHECK(dead.hpLeft == 0);
  CHECK(dead.squashedTotal == 1);
  CHECK(combatAt(seed, FIGHT_SECONDS + 0.1, true).squashedTotal == 1);   // the next bug, not squashed yet
  CHECK(combatAt(seed, 4 * 3600.0, true).squashedTotal == static_cast<std::int64_t>(4 * 3600.0 / FIGHT_SECONDS));
}

TEST_CASE("the same session and time always give the same scene", "[unit][coding-sessions]") {
  const auto seed = wildsSeed("session-2");
  for (double t : {0.0, 5.5, 123.4, 3599.9}) {
    const auto a = combatAt(seed, t, true);
    const auto b = combatAt(seed, t, true);
    CHECK(a.monster == b.monster);
    CHECK(a.hpLeft == b.hpLeft);
    CHECK(skillingAt(seed, t).tally.logs == skillingAt(seed, t).tally.logs);
  }
  CHECK(wildsSeed("a") != wildsSeed("b"));
}

TEST_CASE("skilling gathers one resource per action and rotates every ten minutes", "[unit][coding-sessions]") {
  const auto seed = wildsSeed("session-3");
  for (double t : {0.0, 4.9, 5.0, 599.0, 600.0, 1234.5, 4 * 3600.0}) {
    const auto f = skillingAt(seed, t);
    CHECK(f.tally.logs + f.tally.ore + f.tally.fish == static_cast<std::int64_t>(t / SKILL_ACTION_SECONDS + 1e-9));
    CHECK(f.progress >= 0.0);
    CHECK(f.progress < 1.0);
  }
  CHECK(skillingAt(seed, 10.0).spot == skillingAt(seed, 590.0).spot);
  CHECK(skillingAt(seed, 10.0).spot != skillingAt(seed, 610.0).spot);
  // Tallies only grow.
  std::int64_t last = 0;
  for (double t = 0; t < 7200; t += 37.0) {
    const auto f = skillingAt(seed, t);
    const auto sum = f.tally.logs + f.tally.ore + f.tally.fish;
    CHECK(sum >= last);
    last = sum;
  }
  CHECK(skillName(SkillSpot::Pond) == "Fishing");
  CHECK(skillResource(SkillSpot::Rock) == "Ore");
}
