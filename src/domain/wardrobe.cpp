#include "domain/wardrobe.hpp"

#include <algorithm>

#include "domain/palette.hpp"

namespace as::domain {

namespace {
std::string levelText(int level) { return "Requires Questing level " + std::to_string(level); }
}  // namespace

std::vector<WardrobeEntry> wardrobe(const Loadout& worn, const OwnedSet& owned, bool kiroweenActive, int level,
                                    std::optional<Slot> slotFilter, bool ownedOnly) {
  std::vector<WardrobeEntry> out;
  for (int s = 0; s <= static_cast<int>(Slot::Feet); ++s) {
    const auto slot = static_cast<Slot>(s);
    if (slotFilter && *slotFilter != slot) continue;
    for (const Item& item : ITEMS()) {
      if (item.slot != slot) continue;
      WardrobeEntry e;
      e.item = &item;
      if (owned.find(item.id) != owned.end()) {
        const auto w = worn.find(slot);
        e.status = (w != worn.end() && w->second == item.id) ? ItemStatus::Equipped : ItemStatus::Owned;
      } else if (const auto* lvl = std::get_if<UnlockLevel>(&item.unlock)) {
        e.status = ItemStatus::LockedLevel;
        e.requirement = levelText(lvl->level);
      } else if (const auto* kw = std::get_if<UnlockKiroween>(&item.unlock)) {
        if (!kiroweenActive) {
          e.status = ItemStatus::ReturnsNextKiroween;
          e.requirement = "Returns next Kiroween";
        } else if (kw->minLevel && level < *kw->minLevel) {
          e.status = ItemStatus::KiroweenNeedsLevel;
          e.requirement = levelText(*kw->minLevel);
        } else {
          e.status = ItemStatus::Claimable;
        }
      } else {
        e.status = ItemStatus::Owned;  // a default item missing from `owned` is still ours by definition
      }
      if (ownedOnly && e.status != ItemStatus::Equipped && e.status != ItemStatus::Owned) continue;
      out.push_back(std::move(e));
    }
  }
  return out;
}

std::vector<std::string> levelUnlockIds(int level) {
  std::vector<std::string> ids;
  for (const Item& item : ITEMS()) {
    if (const auto* lvl = std::get_if<UnlockLevel>(&item.unlock); lvl && lvl->level <= level) {
      ids.emplace_back(item.id);
    }
  }
  return ids;
}

std::string describeAvatar(const Appearance& a, const Loadout& loadout) {
  const auto color = [](ColorRegion r, int i) { return std::string(swatchFor(r, i).name); };
  std::string text = std::string("Body type ") + (a.bodyType == BodyType::A ? "A" : "B") + ". ";
  text += "Hair style " + std::to_string(a.hair + 1) + " (" + color(ColorRegion::Hair, a.colors.hair) + "). ";
  text += a.facialHair == 0 ? "No facial hair. " : "Facial hair style " + std::to_string(a.facialHair) + ". ";
  text += "Torso style " + std::to_string(a.torso + 1) + " (" + color(ColorRegion::Torso, a.colors.torso) + "), ";
  text += "arms style " + std::to_string(a.arms + 1) + ", ";
  text += "legs style " + std::to_string(a.legs + 1) + " (" + color(ColorRegion::Legs, a.colors.legs) + "), ";
  text += color(ColorRegion::Feet, a.colors.feet) + " shoes, " + color(ColorRegion::Skin, a.colors.skin) + " skin. ";

  std::vector<std::string> wearing;
  for (int s = 0; s <= static_cast<int>(Slot::Feet); ++s) {
    const auto it = loadout.find(static_cast<Slot>(s));
    if (it == loadout.end()) continue;
    if (const Item* item = findItem(it->second)) wearing.push_back(std::string(item->name));
  }
  if (wearing.empty()) {
    text += "Wearing nothing special.";
  } else {
    text += "Wearing: ";
    for (std::size_t i = 0; i < wearing.size(); ++i) text += (i ? ", " : "") + wearing[i];
    text += ".";
  }
  return text;
}

}  // namespace as::domain
