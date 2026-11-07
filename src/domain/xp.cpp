#include "domain/xp.hpp"

#include <algorithm>
#include <cmath>

#include "domain/wardrobe.hpp"

namespace as::domain {

const std::array<std::int64_t, MAX_QUESTING_LEVEL + 1>& xpTable() {
  static const std::array<std::int64_t, MAX_QUESTING_LEVEL + 1> TABLE = [] {
    std::array<std::int64_t, MAX_QUESTING_LEVEL + 1> t{};
    for (int level = 1; level <= MAX_QUESTING_LEVEL; ++level) {
      t[static_cast<std::size_t>(level)] =
          static_cast<std::int64_t>(std::floor(40.0 * std::pow(static_cast<double>(level - 1), 1.75)));
    }
    return t;
  }();
  return TABLE;
}

int levelForXp(std::int64_t xp) {
  const auto& t = xpTable();
  // First level whose requirement is above xp, minus one. t[1] = 0 so the answer is at least 1.
  const auto it = std::upper_bound(t.begin() + 1, t.end(), std::max<std::int64_t>(xp, 0));
  return static_cast<int>(it - t.begin()) - 1;
}

const char* xpReasonToDb(XpReason reason) {
  switch (reason) {
    case XpReason::QuestLog: return "QUEST_LOG";
    case XpReason::Note: return "NOTE";
    case XpReason::HelpfulReceived: return "HELPFUL_RECEIVED";
    case XpReason::AgentApproved: return "AGENT_APPROVED";
    case XpReason::Seed: return "SEED";
  }
  return "QUEST_LOG";
}

int helpfulXpAllowed(int alreadyTodayXp) {
  const int already = std::max(alreadyTodayXp, 0);
  return already + XP_REWARDS.helpfulReceived <= HELPFUL_DAILY_CAP ? XP_REWARDS.helpfulReceived : 0;
}

std::int64_t guestSeedXp() { return xpTable()[14] - 100; }

std::vector<std::string> newUnlocks(const OwnedSet& owned, int level) {
  std::vector<std::string> out;
  for (auto& id : levelUnlockIds(level)) {
    if (owned.find(id) == owned.end()) out.push_back(std::move(id));
  }
  return out;
}

}  // namespace as::domain
