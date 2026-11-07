#include "domain/stat_table.hpp"

#include <vector>

namespace as::domain {

Agg scopedAgg(const StatTable& table, const StatKey& key, std::optional<QuestType> scope) {
  const auto stat = table.find(storageName(key));
  if (stat == table.end()) return {};
  if (scope) {
    const auto it = stat->second.find(storageName(*scope));
    return it == stat->second.end() ? Agg{} : it->second;
  }
  std::vector<Agg> all;
  all.reserve(stat->second.size());
  for (const auto& [type, agg] : stat->second) all.push_back(agg);
  return mergeAgg(all);
}

AgentLevels levelsFor(const StatTable& table, std::optional<QuestType> scope) {
  AgentLevels out;
  const Agg overall = scopedAgg(table, StatKey{Overall{}}, scope);
  out.overall = skillLevel(overall);
  out.overallCount = overall.count;
  for (std::size_t i = 0; i < SKILLS.size(); ++i) {
    const Agg agg = scopedAgg(table, StatKey{SKILLS[i]}, scope);
    out.skills[i] = skillLevel(agg);
    out.counts[i] = agg.count;
  }
  out.total = totalLevel(out.skills);
  return out;
}

}  // namespace as::domain
