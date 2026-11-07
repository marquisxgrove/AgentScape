#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/appearance.hpp"
#include "domain/loadout.hpp"

namespace as::services {

inline constexpr int MAX_ADVENTURERS = 5;

struct Adventurer {
  std::string id;
  std::string userId;
  std::string name;
  domain::Appearance appearance;
  domain::Loadout loadout;
  bool isActive = false;
  std::chrono::sys_seconds createdAt;
  std::chrono::sys_seconds updatedAt;
  // True when stored data failed validation on load and was repaired (an item left the catalog, a
  // stored value was out of range). The UI prints "Some of your gear has crumbled to dust."
  bool repaired = false;
};

// Creates, saves, switches and deletes Adventurers, and claims Kiroween gear (character-creator
// Requirements 1, 6 and 7). Each call validates with the same domain rules the UI uses, checks that the
// caller owns the target, and runs in one transaction.
class AdventurerService {
 public:
  AdventurerService(db::Database& db, const core::Clock& clock, core::Rng& rng) : db_(db), clock_(clock), rng_(rng) {}

  // Creates an Adventurer with the default look and gear. The user's first one becomes active.
  // Rejects invalid, taken (case/separator-insensitive, across all users) and blocklisted names with
  // "That name is not available.", and a 6th Adventurer with "You can't have more than 5 Adventurers."
  Result<Adventurer> createAdventurer(const std::string& userId, std::string_view name);

  // Re-validates the Appearance and Loadout (unowned or mis-slotted items are refused) and persists.
  Result<Adventurer> saveAdventurer(const std::string& userId, const std::string& id,
                                    const domain::Appearance& appearance, const domain::Loadout& loadout);

  // Makes `id` the user's one active Adventurer.
  Result<Adventurer> setActiveAdventurer(const std::string& userId, const std::string& id);

  // `confirmName` must match the Adventurer's name (compared by nameKey). If the deleted one was
  // active, the most recently updated remaining Adventurer becomes active.
  Result<void> deleteAdventurer(const std::string& userId, const std::string& id, std::string_view confirmName);

  // Claims an unclaimed Kiroween item. Only while `kiroweenActive`; items with a level gate need
  // `userLevel` >= that level. The unlock is permanent. Claiming twice is a harmless no-op.
  Result<void> claimKiroweenItem(const std::string& userId, std::string_view itemId, bool kiroweenActive,
                                 int userLevel);

  // Records an unlock earned elsewhere (level-ups, grants). Idempotent.
  Result<void> grantItem(const std::string& userId, std::string_view itemId, std::string_view source);

  Result<std::vector<Adventurer>> list(const std::string& userId);  // oldest first
  Result<std::optional<Adventurer>> getActive(const std::string& userId);
  // Default items plus the user's unlocks.
  Result<domain::OwnedSet> ownedItems(const std::string& userId);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
};

}  // namespace as::services
