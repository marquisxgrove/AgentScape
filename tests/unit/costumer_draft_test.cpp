#include <catch2/catch_test_macros.hpp>

#include "core/rng.hpp"
#include "ui/costumer_draft.hpp"

using namespace as::ui;
using as::domain::ColorRegion;
using as::domain::Slot;

namespace {
CostumerDraft fresh() {
  CostumerDraft d;
  d.load(as::domain::Appearance{}, as::domain::defaultLoadout(), as::domain::defaultOwnedSet());
  return d;
}
}  // namespace

TEST_CASE("a freshly loaded draft is clean", "[unit][costumer]") {
  CHECK_FALSE(fresh().dirty());
}

TEST_CASE("arrows wrap around in both directions for every option", "[unit][costumer]") {
  auto d = fresh();
  for (const auto o : {StyleOption::BodyType, StyleOption::Hair, StyleOption::FacialHair, StyleOption::Torso,
                       StyleOption::Arms, StyleOption::Legs}) {
    CHECK(d.value(o) == 0);
    d.cycle(o, -1);
    CHECK(d.value(o) == d.count(o) - 1);  // before the first is the last
    d.cycle(o, 1);
    CHECK(d.value(o) == 0);               // after the last is the first
  }
  CHECK(d.count(StyleOption::BodyType) == 2);
  d.cycle(StyleOption::BodyType, 1);
  CHECK(d.appearance().bodyType == as::domain::BodyType::B);
  CHECK(d.dirty());
}

TEST_CASE("colour swatches set a colour and ignore invalid choices", "[unit][costumer]") {
  auto d = fresh();
  d.setColor(ColorRegion::Hair, 5);
  CHECK(d.color(ColorRegion::Hair) == 5);
  d.setColor(ColorRegion::Hair, 99);
  d.setColor(ColorRegion::Hair, -1);
  CHECK(d.color(ColorRegion::Hair) == 5);
  d.setColor(ColorRegion::Feet, 5);   // feet have 6 options
  CHECK(d.color(ColorRegion::Feet) == 5);
  d.setColor(ColorRegion::Feet, 6);
  CHECK(d.color(ColorRegion::Feet) == 5);
}

TEST_CASE("Randomise yields a valid look and keeps the gear", "[unit][costumer]") {
  auto d = fresh();
  as::core::SplitMix64 rng(3);
  d.equip("wooden_stick");
  const auto gear = d.loadout();
  for (int i = 0; i < 50; ++i) {
    d.randomise(rng);
    CHECK(as::domain::validateAppearance(d.appearance()).has_value());
    CHECK(d.loadout() == gear);
  }
}

TEST_CASE("rotation cycles S, E, N, W and wraps", "[unit][costumer]") {
  using F = as::domain::Facing;
  auto d = fresh();
  CHECK(d.facing() == F::S);
  d.rotate(1); CHECK(d.facing() == F::E);
  d.rotate(1); CHECK(d.facing() == F::N);
  d.rotate(1); CHECK(d.facing() == F::W);
  d.rotate(1); CHECK(d.facing() == F::S);
  d.rotate(-1); CHECK(d.facing() == F::W);
}

TEST_CASE("equipping follows the domain rules and explains refusals", "[unit][costumer]") {
  auto d = fresh();
  const auto refused = d.equip("wizard_hat");  // level-gated, not owned
  REQUIRE(refused.has_value());
  CHECK(*refused == "You don't have that item yet.");
  CHECK(d.loadout().count(Slot::Head) == 0);
  CHECK_FALSE(d.dirty());

  auto owned = d.owned();
  owned.insert("wizard_hat");
  owned.insert("prompt_staff");
  owned.insert("codex_tome");
  d.setOwned(owned);
  CHECK_FALSE(d.equip("wizard_hat").has_value());
  CHECK(d.loadout().at(Slot::Head) == "wizard_hat");
  CHECK(d.dirty());

  // Two-handed weapon and shield are mutually exclusive.
  CHECK_FALSE(d.equip("codex_tome").has_value());
  CHECK_FALSE(d.equip("prompt_staff").has_value());
  CHECK(d.loadout().count(Slot::Shield) == 0);
  CHECK_FALSE(d.equip("codex_tome").has_value());
  CHECK(d.loadout().count(Slot::Weapon) == 0);

  d.unequip(Slot::Head);
  CHECK(d.loadout().count(Slot::Head) == 0);
}

TEST_CASE("dirty tracks the saved state, and undoing an edit makes it clean again", "[unit][costumer]") {
  auto d = fresh();
  d.cycle(StyleOption::Hair, 1);
  CHECK(d.dirty());
  d.cycle(StyleOption::Hair, -1);
  CHECK_FALSE(d.dirty());
  d.setColor(ColorRegion::Skin, 4);
  CHECK(d.dirty());
  d.markSaved();
  CHECK_FALSE(d.dirty());
}
