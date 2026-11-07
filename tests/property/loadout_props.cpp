#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "core/rng.hpp"
#include "domain/appearance.hpp"
#include "domain/appearance_json.hpp"
#include "domain/items.hpp"
#include "domain/loadout.hpp"
#include "domain/loadout_json.hpp"
#include "generators/rc_catch.hpp"

using namespace as::domain;

namespace {
std::vector<std::string> allItemIds() {
  std::vector<std::string> ids;
  for (const auto& i : ITEMS()) ids.emplace_back(i.id);
  return ids;
}

// Owned = all default items plus a pseudo-random subset of the rest, from a seed.
OwnedSet ownedFrom(std::uint64_t seed) {
  as::core::SplitMix64 rng(seed);
  OwnedSet owned;
  for (const auto& i : ITEMS()) {
    if (isDefault(i) || rng.below(2) == 0) owned.insert(std::string(i.id));
  }
  return owned;
}

// Apply a scripted sequence of equip/unequip ops and return the final loadout.
Loadout applyOps(std::uint64_t seed, const OwnedSet& owned, int count) {
  as::core::SplitMix64 rng(seed);
  const auto ids = allItemIds();
  Loadout loadout;
  for (int i = 0; i < count; ++i) {
    if (rng.below(3) == 0) {
      loadout = unequip(loadout, static_cast<Slot>(rng.below(9)));
    } else {
      const auto& id = ids[rng.below(ids.size())];
      if (auto r = equip(loadout, id, owned)) loadout = *r;
    }
  }
  return loadout;
}
}  // namespace

// Validates: character-creator Requirement 7.1
TEST_CASE("Property 1: Appearance and loadout round-trip", "[property][character-creator]") {
  as::testing::checkProperty("appearance and loadout survive dump/parse", [](const std::uint64_t seed) {
    as::core::SplitMix64 rng(seed);
    const Appearance a = randomAppearance(rng);
    const nlohmann::json ja = a;
    RC_ASSERT(nlohmann::json::parse(ja.dump()).get<Appearance>() == a);

    const Loadout l = applyOps(seed, ownedFrom(seed), 8);
    const nlohmann::json jl = l;
    RC_ASSERT(nlohmann::json::parse(jl.dump()).get<Loadout>() == l);
  });
}

// Validates: character-creator Requirements 4.1, 4.2
TEST_CASE("Property 2: Slot integrity", "[property][character-creator]") {
  as::testing::checkProperty("every entry's item belongs to its slot key", [](const std::uint64_t seed) {
    const Loadout l = applyOps(seed, ownedFrom(seed), 12);
    for (const auto& [slot, itemId] : l) {
      const Item* item = findItem(itemId);
      RC_ASSERT(item != nullptr);
      RC_ASSERT(item->slot == slot);
    }
  });
}

// Validates: character-creator Requirement 4.3
TEST_CASE("Property 3: Two-handed exclusivity", "[property][character-creator]") {
  as::testing::checkProperty("never a two-handed weapon and a shield together", [](const std::uint64_t seed) {
    const Loadout l = applyOps(seed, ownedFrom(seed), 12);
    const auto w = l.find(Slot::Weapon);
    const bool twoHanded = w != l.end() && findItem(w->second) != nullptr && findItem(w->second)->twoHanded;
    if (twoHanded) RC_ASSERT(l.find(Slot::Shield) == l.end());
  });
}

// Validates: character-creator Requirement 4.4
TEST_CASE("Property 4: Locked items are never equipped", "[property][character-creator]") {
  as::testing::checkProperty("equipped ids are owned; equipping an unowned id is refused", [](const std::uint64_t seed) {
    const OwnedSet owned = ownedFrom(seed);
    const Loadout l = applyOps(seed, owned, 12);
    for (const auto& [slot, itemId] : l) RC_ASSERT(owned.find(itemId) != owned.end());

    // Equipping something not owned returns NotUnlocked and does not change the loadout.
    for (const auto& i : ITEMS()) {
      if (owned.find(std::string(i.id)) == owned.end()) {
        const auto r = equip(l, i.id, owned);
        RC_ASSERT(!r.has_value());
        RC_ASSERT(r.error() == EquipError::NotUnlocked);
        break;
      }
    }
  });
}

// Validates: character-creator Requirement 4.5
TEST_CASE("Property 5: Hiding does not mutate appearance", "[property][character-creator]") {
  as::testing::checkProperty("drawOrder omits hidden layers and leaves appearance unchanged",
                             [](const std::uint64_t seed) {
                               as::core::SplitMix64 rng(seed);
                               const Appearance a = randomAppearance(rng);
                               const Appearance copy = a;
                               const Loadout l = applyOps(seed, ownedFrom(seed), 10);
                               const auto hidden = hiddenLayers(l);
                               for (const auto facing : {Facing::S, Facing::E, Facing::N, Facing::W}) {
                                 const auto order = drawOrder(a, l, facing);
                                 // No drawn key corresponds to a hidden layer.
                                 for (const auto key : order) {
                                   if (key == LayerKey::Hair) RC_ASSERT(hidden.count(Layer::Hair) == 0);
                                   if (key == LayerKey::FacialHair) RC_ASSERT(hidden.count(Layer::FacialHair) == 0);
                                   if (key == LayerKey::Arms) RC_ASSERT(hidden.count(Layer::Arms) == 0);
                                   if (key == LayerKey::Hands) RC_ASSERT(hidden.count(Layer::Hands) == 0);
                                   if (key == LayerKey::Legs) RC_ASSERT(hidden.count(Layer::Legs) == 0);
                                   if (key == LayerKey::Feet) RC_ASSERT(hidden.count(Layer::Feet) == 0);
                                   if (key == LayerKey::Torso) RC_ASSERT(hidden.count(Layer::Torso) == 0);
                                 }
                               }
                               RC_ASSERT(a == copy);
                             });
}

// Validates: character-creator Requirement 3.1
TEST_CASE("Property 6: Draw order is a filtered permutation", "[property][character-creator]") {
  as::testing::checkProperty("drawOrder is a dup-free subsequence of the table", [](const std::uint64_t seed) {
    as::core::SplitMix64 rng(seed);
    const Appearance a = randomAppearance(rng);
    const Loadout l = applyOps(seed, ownedFrom(seed), 10);
    for (const auto facing : {Facing::S, Facing::E, Facing::N, Facing::W}) {
      const auto table = drawOrderTable(facing);
      const auto order = drawOrder(a, l, facing);
      // No duplicates.
      std::set<LayerKey> seen;
      for (const auto k : order) RC_ASSERT(seen.insert(k).second);
      // Subsequence of the table (same relative order).
      std::size_t ti = 0;
      for (const auto k : order) {
        while (ti < table.size() && table[ti] != k) ++ti;
        RC_ASSERT(ti < table.size());
        ++ti;
      }
    }
  });
}
