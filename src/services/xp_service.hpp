#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/xp.hpp"
#include "services/activity_service.hpp"

namespace as::services {

// What one XP award did. `awarded` is false when the same (user, reason, ref) was already in the ledger, so a
// retry never pays twice; everything else is then zero or empty.
struct XpGrant {
  bool awarded = false;
  int amount = 0;
  std::int64_t totalXp = 0;                  // after the award
  int levelBefore = 1;
  int levelAfter = 1;
  std::vector<std::string> unlocked;         // item ids newly unlocked by reaching levelAfter
  std::vector<ActivityEvent> activity;       // recorded in the transaction (level milestones); announce after commit
};

// The Questing XP ledger (quest-logs Requirements 5 and 6). Every write method must run inside the caller's open
// transaction, so an XP change commits (or rolls back) together with the action that earned it. The caller
// publishes the matching events after commit; see publishAfterCommit.
class XpService {
 public:
  XpService(db::Database& db, const core::Clock& clock, core::Rng& rng, ActivityService& activity)
      : db_(db), clock_(clock), rng_(rng), activity_(activity) {}

  // Adds an XP_Entry (unique by user + reason + ref), raises total_xp, and on a level change grants the level
  // unlocks and records a milestone ActivityEvent for every multiple of 10 (and 99) reached.
  Result<XpGrant> award(const std::string& userId, domain::XpReason reason, const std::string& refId, int amount);

  // Raises the (user, reason, ref) entry to `amount`, creating it if needed, and pays only the difference, with the
  // same level, unlock and milestone handling as award(). An entry is never lowered: a smaller or equal amount pays
  // nothing. Used for entries that grow over time (a coding session's XP for one day).
  Result<XpGrant> raiseTo(const std::string& userId, domain::XpReason reason, const std::string& refId, int amount);

  // Removes the ledger rows that reference one Quest Log (its creation XP and any helpful XP) and corrects each
  // affected user's total_xp. Unlocks already granted are kept (Requirement 6.2). Returns the users affected.
  Result<std::vector<std::string>> revokeForQuestLog(const std::string& questLogId);

  // Removes one ledger row (used when a Helpful vote is withdrawn). Returns true if a row was removed.
  Result<bool> revoke(const std::string& userId, domain::XpReason reason, const std::string& refId);

  // XP of one reason given to the user since the start of the UTC day containing `now`.
  Result<int> todaysXp(const std::string& userId, domain::XpReason reason);

  Result<std::int64_t> totalXp(const std::string& userId);

  // Recomputes every user's total_xp from the ledger (the `--repair-xp` flag). Returns how many were wrong.
  Result<int> repairTotals();

 private:
  Result<XpGrant> applyGain(const std::string& userId, const std::string& userName, int gain);

  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  ActivityService& activity_;
};

// Publishes what an award did, AFTER the transaction that made it has committed: XpChanged, ItemUnlocked for each
// new item, and the milestone ActivityEvents it recorded. Does nothing for a grant that paid nothing.
void publishGrant(app::EventBus& bus, ActivityService& activity, const std::string& userId, const XpGrant& grant);

}  // namespace as::services
