#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/loadout.hpp"

namespace as::domain {

// Questing levels run 1..99. (Not the same as the Skill level cap in skills.hpp, which is also 99 by coincidence.)
inline constexpr int MAX_QUESTING_LEVEL = 99;

// XP_TABLE[L] = floor(40 * (L - 1)^1.75) for L = 1..99; level 1 is 0 XP. Index 0 is unused (0).
const std::array<std::int64_t, MAX_QUESTING_LEVEL + 1>& xpTable();

// The largest level L in [1, 99] with xpTable()[L] <= xp. Negative XP counts as 0.
int levelForXp(std::int64_t xp);

enum class XpReason { QuestLog, Note, HelpfulReceived, AgentApproved, Seed };
const char* xpReasonToDb(XpReason reason);   // "QUEST_LOG", "NOTE", "HELPFUL_RECEIVED", "AGENT_APPROVED", "SEED"

struct XpRewards {
  int questLog = 250;
  int note = 50;
  int helpfulReceived = 25;
  int agentApproved = 500;
};
inline constexpr XpRewards XP_REWARDS{};
inline constexpr int HELPFUL_DAILY_CAP = 250;   // per author per UTC day

// Helpful XP an author may still be given today, given what they already got today: 25, or 0 once another 25
// would pass the cap. The total for a day therefore never exceeds HELPFUL_DAILY_CAP.
int helpfulXpAllowed(int alreadyTodayXp);

// The XP a new Guest starts with: Questing level 13, one Quest Log (250 XP) short of level 14 or just over it.
std::int64_t guestSeedXp();

// Ids of the level-rule items reached at `level` that the user does not own yet, in catalog order.
// Granting them and asking again yields nothing (Requirement 6.3); a higher level only ever adds items.
std::vector<std::string> newUnlocks(const OwnedSet& owned, int level);

}  // namespace as::domain
