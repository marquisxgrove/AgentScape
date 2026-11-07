#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/appearance.hpp"
#include "domain/items.hpp"
#include "domain/loadout.hpp"

namespace as::domain {

// What the wardrobe shows for one catalog item (character-creator Requirements 5 and 6).
enum class ItemStatus {
  Equipped,             // owned and currently worn
  Owned,                // owned: can be equipped
  Claimable,            // Kiroween item, event is on, requirements met: "Claim"
  LockedLevel,          // not owned: "Requires Questing level N"
  KiroweenNeedsLevel,   // Kiroween item, event is on, but the extra level gate isn't met
  ReturnsNextKiroween,  // Kiroween item, event is over: "Returns next Kiroween"
};

struct WardrobeEntry {
  const Item* item = nullptr;
  ItemStatus status = ItemStatus::Owned;
  std::string requirement;  // user-facing text for locked states (empty otherwise)
};

// All catalog items (or one slot's) with their state. Order: slots in enum order, catalog order within.
// ownedOnly keeps only Equipped and Owned items.
std::vector<WardrobeEntry> wardrobe(const Loadout& worn, const OwnedSet& owned, bool kiroweenActive, int level,
                                    std::optional<Slot> slotFilter, bool ownedOnly);

// Ids of every UnlockLevel item reached at `level` (items with level <= level), in catalog order. The
// level-up flow grants exactly these; a repeat call after the grant yields nothing new.
std::vector<std::string> levelUnlockIds(int level);

// Plain-text description of the look and gear, for the "Describe" toggle (Req 3.4).
std::string describeAvatar(const Appearance& appearance, const Loadout& loadout);

}  // namespace as::domain
