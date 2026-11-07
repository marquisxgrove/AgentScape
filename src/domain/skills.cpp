#include "domain/skills.hpp"

#include <algorithm>
#include <cmath>

namespace as::domain {

Agg mergeAgg(std::span<const Agg> aggs) {
  Agg total;
  for (const Agg& a : aggs) {
    total.sum += a.sum;
    total.count += a.count;
  }
  return total;
}

Agg aggOf(std::span<const int> ratings) {
  Agg agg;
  for (const int r : ratings) {
    agg.sum += std::clamp(r, 1, 5);
    ++agg.count;
  }
  return agg;
}

std::optional<int> skillLevel(const Agg& agg) {
  if (agg.count < MIN_RATINGS) return std::nullopt;
  const double bayes = (static_cast<double>(PRIOR_WEIGHT * PRIOR_MEAN) + static_cast<double>(agg.sum)) /
                       static_cast<double>(PRIOR_WEIGHT + agg.count);
  const long raw = 1 + std::lround((bayes - 1.0) / 4.0 * 98.0);
  return static_cast<int>(std::clamp<long>(raw, MIN_LEVEL, MAX_LEVEL));
}

int totalLevel(std::span<const std::optional<int>> levels) {
  int total = 0;
  for (const auto& level : levels) total += level.value_or(1);
  return total;
}

namespace {
struct SkillNames {
  Skill skill;
  const char* display;
  const char* storage;
};
constexpr std::array<SkillNames, 10> SKILL_NAMES{{
    {Skill::Coding, "Coding", "CODING"},
    {Skill::Reasoning, "Reasoning", "REASONING"},
    {Skill::Writing, "Writing", "WRITING"},
    {Skill::Research, "Research", "RESEARCH"},
    {Skill::Creativity, "Creativity", "CREATIVITY"},
    {Skill::ToolUse, "Tool Use", "TOOL_USE"},
    {Skill::Context, "Context", "CONTEXT"},
    {Skill::Reliability, "Reliability", "RELIABILITY"},
    {Skill::Speed, "Speed", "SPEED"},
    {Skill::Value, "Value", "VALUE"},
}};

struct TypeNames {
  QuestType type;
  const char* display;
  const char* storage;
};
constexpr std::array<TypeNames, 8> TYPE_NAMES{{
    {QuestType::Coding, "Coding", "CODING"},
    {QuestType::Debugging, "Debugging", "DEBUGGING"},
    {QuestType::Writing, "Writing", "WRITING"},
    {QuestType::Research, "Research", "RESEARCH"},
    {QuestType::Data, "Data", "DATA"},
    {QuestType::Brainstorming, "Brainstorming", "BRAINSTORMING"},
    {QuestType::Agentic, "Agentic", "AGENTIC"},
    {QuestType::Other, "Other", "OTHER"},
}};
}  // namespace

const char* displayName(Skill skill) {
  for (const auto& n : SKILL_NAMES) {
    if (n.skill == skill) return n.display;
  }
  return "?";
}

const char* displayName(QuestType type) {
  for (const auto& n : TYPE_NAMES) {
    if (n.type == type) return n.display;
  }
  return "?";
}

std::string storageName(const StatKey& key) {
  if (std::holds_alternative<Overall>(key)) return "OVERALL";
  const Skill skill = std::get<Skill>(key);
  for (const auto& n : SKILL_NAMES) {
    if (n.skill == skill) return n.storage;
  }
  return "?";
}

std::optional<StatKey> statKeyFromStorage(std::string_view s) {
  if (s == "OVERALL") return StatKey{Overall{}};
  for (const auto& n : SKILL_NAMES) {
    if (s == n.storage) return StatKey{n.skill};
  }
  return std::nullopt;
}

std::string storageName(QuestType type) {
  for (const auto& n : TYPE_NAMES) {
    if (n.type == type) return n.storage;
  }
  return "?";
}

std::optional<QuestType> questTypeFromStorage(std::string_view s) {
  for (const auto& n : TYPE_NAMES) {
    if (s == n.storage) return n.type;
  }
  return std::nullopt;
}

}  // namespace as::domain
