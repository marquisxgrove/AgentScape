#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "game/kiroween_fx.hpp"

using namespace as::game;

TEST_CASE("fog drifts and wraps within the tile when animated", "[unit][kiroween]") {
  Drifter d{60, 60, 10, 10};
  stepDrifter(d, 1.0, 64, 64, /*animate=*/true);
  CHECK(d.x == 6.0F);  // 60 + 10 wrapped into [0,64)
  CHECK(d.y == 6.0F);
}

TEST_CASE("reduce-motion holds the fog perfectly still", "[unit][kiroween]") {
  Drifter d{20, 30, 10, 10};
  stepDrifter(d, 5.0, 64, 64, /*animate=*/false);
  CHECK(d.x == 20.0F);
  CHECK(d.y == 30.0F);
}

TEST_CASE("pumpkin flicker stays within bounds, steady when still", "[unit][kiroween]") {
  Pumpkin p;
  for (int i = 0; i < 200; ++i) {
    stepPumpkin(p, 0.016, /*animate=*/true);
    CHECK(p.flicker >= 0.6F);
    CHECK(p.flicker <= 1.0F);
  }
  Pumpkin still;
  still.flicker = 0.3F;
  stepPumpkin(still, 1.0, /*animate=*/false);
  CHECK(still.flicker == 1.0F);
}

TEST_CASE("bats fly, bob, and flap only when animated", "[unit][kiroween]") {
  auto bats = makeBats(8, 300, 120, 42);
  REQUIRE(bats.size() == 8);
  const auto before = bats[0];
  stepBat(bats[0], 0.5, 300, 16, /*animate=*/true);
  CHECK(bats[0].x != before.x);               // moved horizontally
  CHECK(std::abs(bats[0].y - before.baseY) <= before.bob + 0.001F);  // bobs within amplitude

  Bat frozen = before;
  stepBat(frozen, 0.5, 300, 16, /*animate=*/false);
  CHECK(frozen.x == before.x);
  CHECK(frozen.y == before.y);
}

TEST_CASE("makeBats is deterministic for a seed", "[unit][kiroween]") {
  const auto a = makeBats(5, 400, 200, 7);
  const auto b = makeBats(5, 400, 200, 7);
  REQUIRE(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    CHECK(a[i].x == b[i].x);
    CHECK(a[i].baseY == b[i].baseY);
    CHECK(a[i].speed == b[i].speed);
  }
}
