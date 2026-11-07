#pragma once

#include <array>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "domain/codex_validation.hpp"
#include "domain/skills.hpp"

namespace as::domain {

using Stars = int;   // 1..5, enforced by the validators

// The ratings one Quest Log contributes to the Codex: its overall rating plus any Skill ratings, in one Quest Type.
// An excluded Quest Log contributes nothing (a Keeper took its ratings out of the aggregates; Requirement 2.3).
struct RatingSet {
  QuestType questType = QuestType::Other;
  Stars overall = 3;
  std::map<Skill, Stars> skills;
  bool excluded = false;

  bool operator==(const RatingSet&) const = default;
};

// One change to one aggregate (matches services::RatingDelta, with the Quest Type attached).
// A new rating r is {+r, +1}, deleting it is {-r, -1}, and changing it from r to r2 is {r2 - r, 0}.
struct Delta {
  StatKey skill = Overall{};
  QuestType questType = QuestType::Other;
  std::int64_t sumDelta = 0;
  std::int64_t countDelta = 0;

  bool operator==(const Delta&) const = default;
};

// What moving a Quest Log from `before` to `after` does to the aggregates. nullopt means "no Quest Log" (before a
// create, after a delete), and an excluded RatingSet counts as nullopt. Moving between Quest Types is handled: the
// old type loses the ratings and the new one gains them. The result is in a canonical order (by Quest Type, then
// Overall first and the Skills in display order) with no all-zero entries, so equal changes compare equal.
std::vector<Delta> ratingDeltas(const std::optional<RatingSet>& before, const std::optional<RatingSet>& after);

// Adds lists of deltas together, merging entries for the same (Skill, Quest Type) and dropping the ones that
// cancel out. Same canonical order as ratingDeltas.
std::vector<Delta> sumDeltas(std::span<const std::vector<Delta>> lists);

// --- Quest Log input (Requirement 1.1) ----------------------------------------------------------------------

inline constexpr std::size_t MAX_VERSION_LEN = 40;
inline constexpr std::size_t MIN_TITLE = 5;
inline constexpr std::size_t MAX_TITLE = 80;
inline constexpr std::size_t MIN_BODY = 20;
inline constexpr std::size_t MAX_BODY = 2000;
inline constexpr std::size_t MAX_TAGS_PER_SIDE = 3;

inline constexpr std::array<std::string_view, 8> PRO_TAGS{"Accurate", "Fast", "Clear explanations", "Followed instructions",
                                                          "Clean code", "Good with large context", "Great value", "Creative"};
inline constexpr std::array<std::string_view, 8> CON_TAGS{"Hallucinated", "Slow", "Too verbose", "Ignored instructions",
                                                          "Outdated knowledge", "Lost context", "Expensive", "Over-refused"};

struct QuestLogInput {
  std::string agentId;
  std::string version;                 // optional; blank = none
  QuestType questType = QuestType::Other;
  Stars overall = 3;
  std::map<Skill, Stars> skills;       // 0..10 entries
  std::string title;
  std::string body;                    // plain text; line breaks are kept
  bool recommend = false;
  std::vector<std::string> pros;       // from PRO_TAGS, at most 3, no repeats
  std::vector<std::string> cons;       // from CON_TAGS, at most 3, no repeats

  bool operator==(const QuestLogInput&) const = default;
};

using ValidationErrors = std::vector<FieldError>;

// Checks every field and reports every problem (not just the first), so the form can mark them all. On success
// returns the cleaned input: text trimmed, "\r\n" turned into "\n". The agent must also be approved, but that
// needs the database and is checked by the service.
Result<QuestLogInput, ValidationErrors> validateQuestLogInput(const QuestLogInput& input);

// The RatingSet a Quest Log input contributes (not excluded).
RatingSet ratingSetOf(const QuestLogInput& input);

// --- Storage forms ---------------------------------------------------------------------------------------------

// {"CODING":4,"TOOL_USE":5} (the `ratings` column). Lenient on read: bad entries are dropped.
std::string ratingsToJson(const std::map<Skill, Stars>& skills);
std::map<Skill, Stars> ratingsFromJson(std::string_view json);

// ["Accurate","Slow"] (the `tags` column: pros and cons together; the two lists share no names, so each tag's side
// is known from the lists). Lenient on read: unknown tags are dropped.
std::string tagsToJson(const std::vector<std::string>& pros, const std::vector<std::string>& cons);
void tagsFromJson(std::string_view json, std::vector<std::string>& pros, std::vector<std::string>& cons);

// The whole input as one JSON object, and back (nullopt if the text is not a JSON object of the right shape).
std::string questLogInputToJson(const QuestLogInput& input);
std::optional<QuestLogInput> questLogInputFromJson(std::string_view json);

}  // namespace as::domain
