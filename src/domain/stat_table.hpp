#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>

#include "domain/skills.hpp"

namespace as::domain {

// All of one Agent's rating aggregates, as stored: stat storage name ("CODING", "OVERALL") -> quest type
// storage name ("DEBUGGING") -> (sum, count). Pure data: loaded by the service, interpreted here.
using StatTable = std::map<std::string, std::map<std::string, Agg>>;

// The aggregate for one stat in one scope; nullopt scope = "All quest types" (the sum over every type).
Agg scopedAgg(const StatTable& table, const StatKey& key, std::optional<QuestType> scope);

// Everything an examine card, list row or comparison needs for one scope.
struct AgentLevels {
  std::optional<int> overall;                  // Quest Log overall rating as a level; not part of totalLevel
  std::int64_t overallCount = 0;               // number of Quest Logs in this scope
  std::array<std::optional<int>, 10> skills;   // SKILLS order; nullopt = unranked
  std::array<std::int64_t, 10> counts{};       // ratings behind each Skill
  int total = 10;                              // totalLevel: unranked counts as 1
};

AgentLevels levelsFor(const StatTable& table, std::optional<QuestType> scope);

}  // namespace as::domain
