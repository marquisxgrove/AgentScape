#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "domain/wardrobe.hpp"

using namespace as::domain;

namespace {
const WardrobeEntry* find(const std::vector<WardrobeEntry>& v, std::string_view id) {
  const auto it = std::find_if(v.begin(), v.end(), [&](const WardrobeEntry& e) { return e.item->id == id; });
  return it == v.end() ? nullptr : &*it;
}
}  // namespace

TEST_CASE("the wardrobe lists the whole catalog once each, grouped by slot", "[unit][wardrobe]") {
  const auto all = wardrobe({}, defaultOwnedSet(), false, 1, std::nullopt, false);
  CHECK(all.size() == ITEMS().size());
  int lastSlot = -1;
  for (const auto& e : all) {
    CHECK(static_cast<int>(e.item->slot) >= lastSlot);
    lastSlot = static_cast<int>(e.item->slot);
  }
  const auto heads = wardrobe({}, defaultOwnedSet(), false, 1, Slot::Head, false);
  CHECK(heads.size() == 5);  // hood, wizard hat, witch hat, jack mask, skull mask
  for (const auto& e : heads) CHECK(e.item->slot == Slot::Head);
}

TEST_CASE("owned items are Owned, worn items Equipped", "[unit][wardrobe]") {
  const auto worn = defaultLoadout();
  const auto all = wardrobe(worn, defaultOwnedSet(), false, 1, std::nullopt, false);
  CHECK(find(all, "wooden_stick")->status == ItemStatus::Equipped);
  CHECK(find(all, "plain_tunic")->status == ItemStatus::Equipped);
  OwnedSet owned = defaultOwnedSet();
  owned.insert("quill_sword");
  const auto more = wardrobe(worn, owned, false, 1, std::nullopt, false);
  CHECK(find(more, "quill_sword")->status == ItemStatus::Owned);
}

TEST_CASE("level-gated items show their requirement until owned", "[unit][wardrobe]") {
  const auto all = wardrobe({}, defaultOwnedSet(), false, 9, std::nullopt, false);
  const auto* cape = find(all, "travellers_cape");
  REQUIRE(cape != nullptr);
  CHECK(cape->status == ItemStatus::LockedLevel);
  CHECK(cape->requirement == "Requires Questing level 10");
}

TEST_CASE("Kiroween items: claimable in season, gated by level, otherwise return next year", "[unit][wardrobe]") {
  const auto off = wardrobe({}, defaultOwnedSet(), /*kiroweenActive=*/false, 99, std::nullopt, false);
  CHECK(find(off, "witch_hat")->status == ItemStatus::ReturnsNextKiroween);
  CHECK(find(off, "witch_hat")->requirement == "Returns next Kiroween");
  CHECK(find(off, "reapers_scythe")->status == ItemStatus::ReturnsNextKiroween);

  const auto low = wardrobe({}, defaultOwnedSet(), true, 12, std::nullopt, false);
  CHECK(find(low, "witch_hat")->status == ItemStatus::Claimable);
  CHECK(find(low, "reapers_scythe")->status == ItemStatus::KiroweenNeedsLevel);
  CHECK(find(low, "reapers_scythe")->requirement == "Requires Questing level 13");

  const auto ok = wardrobe({}, defaultOwnedSet(), true, 13, std::nullopt, false);
  CHECK(find(ok, "reapers_scythe")->status == ItemStatus::Claimable);

  // A claimed Kiroween item stays ours after the event ends.
  OwnedSet owned = defaultOwnedSet();
  owned.insert("witch_hat");
  CHECK(find(wardrobe({}, owned, false, 1, std::nullopt, false), "witch_hat")->status == ItemStatus::Owned);
}

TEST_CASE("Owned only hides everything that isn't ours", "[unit][wardrobe]") {
  const auto owned = wardrobe(defaultLoadout(), defaultOwnedSet(), true, 99, std::nullopt, true);
  CHECK(owned.size() == defaultOwnedSet().size());
  for (const auto& e : owned) {
    CHECK((e.status == ItemStatus::Equipped || e.status == ItemStatus::Owned));
  }
}

TEST_CASE("level unlocks are monotone and match the catalog's level rules", "[unit][wardrobe]") {
  CHECK(levelUnlockIds(0).empty());
  CHECK(levelUnlockIds(2).empty());  // nothing is gated below level 3
  const auto at3 = levelUnlockIds(3);
  REQUIRE(at3.size() == 1);
  CHECK(at3[0] == "leather_gloves");
  const auto at13 = levelUnlockIds(13);
  for (const auto& id : at3) CHECK(std::find(at13.begin(), at13.end(), id) != at13.end());
  CHECK(std::find(at13.begin(), at13.end(), "scholar_robe") != at13.end());       // level 12
  CHECK(std::find(at13.begin(), at13.end(), "token_amulet") == at13.end());       // level 15
  CHECK(levelUnlockIds(99).size() > at13.size());
  for (const auto& id : levelUnlockIds(99)) CHECK_FALSE(isKiroween(*findItem(id)));  // never Kiroween gear
}

TEST_CASE("Describe reads the look and the gear in plain text", "[unit][wardrobe]") {
  Appearance a;
  a.hair = 2;
  a.facialHair = 0;
  a.colors.hair = 3;  // chestnut
  a.colors.skin = 2;  // light
  Loadout gear = defaultLoadout();
  gear[Slot::Head] = "witch_hat";
  const auto text = describeAvatar(a, gear);
  CHECK(text.find("Body type A") != std::string::npos);
  CHECK(text.find("Hair style 3 (chestnut)") != std::string::npos);
  CHECK(text.find("No facial hair") != std::string::npos);
  CHECK(text.find("light skin") != std::string::npos);
  CHECK(text.find("Witch's Hat") != std::string::npos);
  CHECK(text.find("Wooden Stick") != std::string::npos);
  CHECK(describeAvatar(a, {}).find("Wearing nothing special.") != std::string::npos);
}
