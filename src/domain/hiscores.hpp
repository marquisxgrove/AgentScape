#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "domain/skills.hpp"
#include "domain/stat_table.hpp"

namespace as::domain {

inline constexpr std::size_t HISCORES_PAGE_SIZE = 25;   // Requirement 7.4

enum class AgentBoardKind { Overall, Total, Skill };

// Which Agent leaderboard: the Overall level, the Total level, or one Skill's level, optionally within one Quest Type.
struct AgentBoard {
  AgentBoardKind kind = AgentBoardKind::Overall;
  Skill skill = Skill::Coding;           // used when kind == Skill
  std::optional<QuestType> scope;        // nullopt = all Quest Types

  bool operator==(const AgentBoard&) const = default;
};

// The score an Agent has on a board, or nullopt if it is unranked there, which keeps it off that board
// (Requirement 7.1). Overall and Skill boards use the level, which needs at least 3 ratings. The Total board counts an
// unranked Skill as 1, so an Agent with no ranked Skill at all is left off it instead of sitting on a floor of 10.
std::optional<int> boardScore(const AgentLevels& levels, const AgentBoard& board);

const char* boardName(const AgentBoard& board);   // "Overall", "Total level", "Coding", ...

// The first entry matching a "search by name" query, as an index into `namesInOrder` (display order). Matching is
// case-insensitive ASCII. A whole-name match beats a name that starts with the query, which beats a name that merely
// contains it; within one kind the earliest in display order wins. A blank query matches nothing.
std::optional<std::size_t> findByName(std::span<const std::string> namesInOrder, std::string_view query);

}  // namespace as::domain
