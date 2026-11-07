#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "domain/skills.hpp"
#include "domain/stat_table.hpp"

namespace as::domain {

inline constexpr std::size_t MIN_COMPARE = 2;   // Requirement 6.1
inline constexpr std::size_t MAX_COMPARE = 3;

struct CompareColumn {
  std::string name;
  AgentLevels levels;  // already computed for the chosen Quest Type scope
};

struct CompareRow {
  std::string label;                       // "Overall", a Skill name, or "Total level"
  std::vector<std::optional<int>> levels;  // one per column; nullopt = unranked
  std::vector<bool> best;                  // one per column: holds the highest ranked level in this row
};

struct Comparison {
  std::string scopeLabel;                  // "All" or a Quest Type name
  std::vector<std::string> names;
  std::vector<CompareRow> rows;            // Overall, the ten Skills in order, then Total level
};

// Builds the side-by-side table. In each row the highest ranked level is marked best, and every column that
// ties it is marked too (Requirement 6.2). An unranked cell is never best; a row where nobody is ranked has
// no best cell.
Comparison compare(std::span<const CompareColumn> columns, std::optional<QuestType> scope);

// The comparison as a plain-text table for the clipboard (Requirement 6.4). Best cells carry a trailing '*',
// unranked cells show '-'. Columns are padded by code points so non-ASCII names line up.
std::string toPlainText(const Comparison& comparison);

}  // namespace as::domain
