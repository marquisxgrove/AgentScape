#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "domain/codex_validation.hpp"
#include "domain/skills.hpp"
#include "domain/stat_table.hpp"

namespace as::domain {

// What the Codex list needs to know about one approved Agent in the chosen Quest Type scope.
struct Candidate {
  std::string name;
  std::string vendor;
  AgentKind kind = AgentKind::Model;
  AgentLevels levels;  // already computed for the scope (levelsFor)
};

enum class SortKey { Total, Skill, Name, QuestLogs };

inline constexpr std::size_t CODEX_PAGE_SIZE = 20;  // Requirement 1.1

struct ListQuery {
  std::string text;                   // case-insensitive search over name and vendor; every word must match
  std::optional<AgentKind> kind;      // filter
  std::optional<std::string> vendor;  // filter, case-insensitive exact match
  SortKey sort = SortKey::Total;
  Skill skill = Skill::Coding;        // which Skill to sort by when sort == Skill
  bool reversed = false;              // flip the natural direction of `sort`
  std::size_t page = 0;               // zero-based
};

// The indices of `candidates` that pass the filters, in display order (the whole result, every page).
// - Search: lower-cased ASCII, split on whitespace; each word must appear in "name vendor".
// - Natural order: Total, Skill and QuestLogs go highest first; Name goes A to Z. `reversed` flips that.
// - Unranked Skill levels always sort last, whichever direction is chosen (Requirement 1.4).
// - Ties break by name (A to Z) and then by original index, so the order is total and stable.
std::vector<std::size_t> selectAndSort(std::span<const Candidate> candidates, const ListQuery& query);

struct PageSlice {
  std::size_t first = 0;   // index of the first item in the result
  std::size_t count = 0;   // items on this page
  std::size_t page = 0;    // the page actually shown (clamped)
  std::size_t pages = 1;   // total pages (at least 1, even when empty)
};
PageSlice paginate(std::size_t total, std::size_t page, std::size_t pageSize = CODEX_PAGE_SIZE);

// The distinct vendors of the candidates, sorted case-insensitively, for the vendor filter.
std::vector<std::string> distinctVendors(std::span<const Candidate> candidates);

}  // namespace as::domain
