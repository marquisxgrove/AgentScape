#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "domain/appearance.hpp"
#include "domain/items.hpp"

namespace as::domain {

// slot -> itemId. At most one item per slot; the item must belong to that slot (Req 4.1).
using Loadout = std::map<Slot, std::string>;

// Account-wide owned item ids (all UnlockDefault items plus the user's unlock rows).
using OwnedSet = std::set<std::string, std::less<>>;

enum class EquipError { UnknownItem, NotUnlocked };

// Every UnlockDefault item id: what a brand-new account owns.
OwnedSet defaultOwnedSet();
// The default item of each slot that has one (Wooden Stick, Plain Tunic, Plain Trousers, Leather Boots).
Loadout defaultLoadout();

// Equips itemId: places it in its own slot, replaces any current occupant (Req 4.2); equipping a
// two-handed weapon removes any shield and vice-versa (Req 4.3). Refuses unowned items, leaving the
// loadout unchanged (Req 4.4).
Result<Loadout, EquipError> equip(const Loadout& current, std::string_view itemId, const OwnedSet& owned);

Loadout unequip(const Loadout& current, Slot slot);

// Drops entries that are unknown, mis-slotted, or unowned; the result is internally consistent.
// Never fails; returns the cleaned loadout (callers log what was dropped).
Loadout validateLoadout(const Loadout& current, const OwnedSet& owned);

// The union of hides-flags of all equipped items.
std::set<Layer> hiddenLayers(const Loadout& loadout);

enum class Facing { S, E, N, W };

// A drawable layer: either an Appearance-derived body layer or an equipped item's slot.
enum class LayerKey {
  CapeItem,
  BaseBody,
  Legs,
  Feet,
  Torso,
  Arms,
  Hands,
  LegsItem,
  FeetItem,
  BodyItem,
  HandsItem,
  NeckItem,
  Hair,
  FacialHair,
  HeadItem,
  ShieldItem,
  WeaponItem,
};

// The full back-to-front layer table for a facing (before filtering).
std::vector<LayerKey> drawOrderTable(Facing facing);

// drawOrderTable filtered to the layers actually present (given appearance + loadout) and not
// hidden by any equipped item. Back-to-front. Appearance is read-only.
std::vector<LayerKey> drawOrder(const Appearance& appearance, const Loadout& loadout, Facing facing);

}  // namespace as::domain
