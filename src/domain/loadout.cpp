#include "domain/loadout.hpp"

#include <algorithm>

namespace as::domain {

namespace {
bool isTwoHanded(const std::string& itemId) {
  const Item* item = findItem(itemId);
  return item != nullptr && item->twoHanded;
}
}  // namespace

Result<Loadout, EquipError> equip(const Loadout& current, std::string_view itemId, const OwnedSet& owned) {
  const Item* item = findItem(itemId);
  if (item == nullptr) return tl::unexpected(EquipError::UnknownItem);
  if (owned.find(itemId) == owned.end()) return tl::unexpected(EquipError::NotUnlocked);

  Loadout next = current;
  next[item->slot] = std::string(itemId);  // replace any current occupant of that slot
  // Two-handed exclusivity (Req 4.3).
  if (item->slot == Slot::Weapon && item->twoHanded) {
    next.erase(Slot::Shield);
  } else if (item->slot == Slot::Shield) {
    const auto w = next.find(Slot::Weapon);
    if (w != next.end() && isTwoHanded(w->second)) next.erase(Slot::Weapon);
  }
  return next;
}

Loadout unequip(const Loadout& current, Slot slot) {
  Loadout next = current;
  next.erase(slot);
  return next;
}

Loadout validateLoadout(const Loadout& current, const OwnedSet& owned) {
  Loadout clean;
  for (const auto& [slot, itemId] : current) {
    const Item* item = findItem(itemId);
    if (item == nullptr || item->slot != slot) continue;       // unknown or mis-slotted
    if (owned.find(itemId) == owned.end()) continue;           // not owned
    clean[slot] = itemId;
  }
  // Resolve two-handed/shield conflicts deterministically: a two-handed weapon wins over a shield.
  const auto w = clean.find(Slot::Weapon);
  if (w != clean.end() && isTwoHanded(w->second)) clean.erase(Slot::Shield);
  return clean;
}

std::set<Layer> hiddenLayers(const Loadout& loadout) {
  std::set<Layer> hidden;
  for (const auto& [slot, itemId] : loadout) {
    const Item* item = findItem(itemId);
    if (item == nullptr) continue;
    for (const Layer l : item->hides) hidden.insert(l);
  }
  return hidden;
}

std::vector<LayerKey> drawOrderTable(Facing facing) {
  using L = LayerKey;
  // Facing S: the canonical back-to-front order from design.md.
  std::vector<L> order{L::CapeItem,  L::BaseBody, L::Legs,      L::Feet,     L::Torso,     L::Arms,
                       L::Hands,     L::LegsItem, L::FeetItem,  L::BodyItem, L::HandsItem, L::NeckItem,
                       L::Hair,      L::FacialHair, L::HeadItem, L::ShieldItem, L::WeaponItem};
  if (facing == Facing::N) {
    // Facing away: the cape is in front; the weapon and shield are behind the body.
    order.erase(std::remove(order.begin(), order.end(), L::CapeItem), order.end());
    order.erase(std::remove(order.begin(), order.end(), L::ShieldItem), order.end());
    order.erase(std::remove(order.begin(), order.end(), L::WeaponItem), order.end());
    // Rebuild: body-back weapon/shield, everything else, cape on top.
    std::vector<L> n{L::WeaponItem, L::ShieldItem, L::BaseBody, L::Legs,       L::Feet,       L::Torso,
                     L::Arms,       L::Hands,       L::LegsItem, L::FeetItem,   L::BodyItem,   L::HandsItem,
                     L::NeckItem,   L::Hair,        L::FacialHair, L::HeadItem, L::CapeItem};
    return n;
  }
  return order;
}

namespace {
// Is a body layer present for this appearance? (Facial hair 0 = none; others always present.)
bool bodyLayerPresent(LayerKey key, const Appearance& a) {
  switch (key) {
    case LayerKey::FacialHair: return a.facialHair != 0;
    case LayerKey::BaseBody:
    case LayerKey::Legs:
    case LayerKey::Feet:
    case LayerKey::Torso:
    case LayerKey::Arms:
    case LayerKey::Hands:
    case LayerKey::Hair: return true;
    default: return false;  // item layers handled separately
  }
}

// Map an item-layer key to the slot that must be occupied for it to appear.
std::optional<Slot> itemSlotFor(LayerKey key) {
  switch (key) {
    case LayerKey::CapeItem: return Slot::Cape;
    case LayerKey::LegsItem: return Slot::Legs;
    case LayerKey::FeetItem: return Slot::Feet;
    case LayerKey::BodyItem: return Slot::Body;
    case LayerKey::HandsItem: return Slot::Hands;
    case LayerKey::NeckItem: return Slot::Neck;
    case LayerKey::HeadItem: return Slot::Head;
    case LayerKey::ShieldItem: return Slot::Shield;
    case LayerKey::WeaponItem: return Slot::Weapon;
    default: return std::nullopt;
  }
}

// Which avatar Layer does a body LayerKey correspond to (for the hidden-layers filter)?
std::optional<Layer> hideableLayerFor(LayerKey key) {
  switch (key) {
    case LayerKey::Hair: return Layer::Hair;
    case LayerKey::FacialHair: return Layer::FacialHair;
    case LayerKey::Arms: return Layer::Arms;
    case LayerKey::Hands: return Layer::Hands;
    case LayerKey::Legs: return Layer::Legs;
    case LayerKey::Feet: return Layer::Feet;
    case LayerKey::Torso: return Layer::Torso;
    default: return std::nullopt;
  }
}
}  // namespace

std::vector<LayerKey> drawOrder(const Appearance& appearance, const Loadout& loadout, Facing facing) {
  const auto hidden = hiddenLayers(loadout);
  std::vector<LayerKey> result;
  for (const LayerKey key : drawOrderTable(facing)) {
    if (const auto slot = itemSlotFor(key)) {
      if (loadout.find(*slot) == loadout.end()) continue;  // no item in that slot
    } else {
      if (!bodyLayerPresent(key, appearance)) continue;
    }
    if (const auto layer = hideableLayerFor(key); layer && hidden.count(*layer) != 0) continue;  // hidden
    result.push_back(key);
  }
  return result;
}

}  // namespace as::domain
