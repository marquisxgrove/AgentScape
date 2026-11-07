#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace as::domain {

// The ten rating dimensions, in display order (product.md).
enum class Skill { Coding, Reasoning, Writing, Research, Creativity, ToolUse, Context, Reliability, Speed, Value };
inline constexpr std::array<Skill, 10> SKILLS{Skill::Coding,   Skill::Reasoning, Skill::Writing, Skill::Research,
                                              Skill::Creativity, Skill::ToolUse, Skill::Context, Skill::Reliability,
                                              Skill::Speed,    Skill::Value};

// The Quest Log's overall rating, kept as its own stat: shown like a combat level, never part of totalLevel.
struct Overall {
  bool operator==(const Overall&) const = default;
};
using StatKey = std::variant<Skill, Overall>;

enum class QuestType { Coding, Debugging, Writing, Research, Data, Brainstorming, Agentic, Other };
inline constexpr std::array<QuestType, 8> QUEST_TYPES{QuestType::Coding,  QuestType::Debugging,
                                                      QuestType::Writing, QuestType::Research,
                                                      QuestType::Data,    QuestType::Brainstorming,
                                                      QuestType::Agentic, QuestType::Other};

// Stored form of a rating aggregate: any scope is computed by adding aggregates (Requirement 3.5).
struct Agg {
  std::int64_t sum = 0;
  std::int64_t count = 0;
  bool operator==(const Agg&) const = default;
};

inline constexpr int PRIOR_MEAN = 3;    // bayes prior mean m
inline constexpr int PRIOR_WEIGHT = 5;  // bayes prior weight C
inline constexpr int MIN_RATINGS = 3;   // fewer than this is unranked
inline constexpr int MIN_LEVEL = 1;
inline constexpr int MAX_LEVEL = 99;

Agg mergeAgg(std::span<const Agg> aggs);
Agg aggOf(std::span<const int> ratings);  // ratings are 1..5; out-of-range values are clamped

// bayes = (C*m + sum) / (C + n); level = clamp(1 + round((bayes - 1) / 4 * 98), 1, 99); nullopt if n < 3.
std::optional<int> skillLevel(const Agg& agg);

// Sum of the Skill levels, where an unranked Skill counts as 1 (range 10..990 for ten Skills).
int totalLevel(std::span<const std::optional<int>> levels);

// Display and storage names. The stored form is upper snake case ("TOOL_USE"); parsing is exact.
const char* displayName(Skill skill);       // "Tool Use"
const char* displayName(QuestType type);    // "Coding"
std::string storageName(const StatKey& key);        // "CODING", "TOOL_USE", "OVERALL"
std::optional<StatKey> statKeyFromStorage(std::string_view s);
std::string storageName(QuestType type);            // "CODING", "BRAINSTORMING"
std::optional<QuestType> questTypeFromStorage(std::string_view s);

}  // namespace as::domain
