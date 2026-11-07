#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "domain/hiscores.hpp"

using namespace as::domain;

namespace {
AgentLevels levels(std::optional<int> overall, std::optional<int> coding, std::optional<int> speed, int total) {
  AgentLevels l;
  l.overall = overall;
  l.skills[static_cast<std::size_t>(Skill::Coding)] = coding;
  l.skills[static_cast<std::size_t>(Skill::Speed)] = speed;
  l.total = total;
  return l;
}
}  // namespace

TEST_CASE("the Overall board uses the Overall level, and an unranked Agent is left off", "[unit][hiscores]") {
  AgentBoard board;   // Overall
  CHECK(boardScore(levels(70, std::nullopt, std::nullopt, 10), board) == std::optional<int>{70});
  CHECK_FALSE(boardScore(levels(std::nullopt, 80, 80, 200), board).has_value());
}

TEST_CASE("a Skill board uses that Skill's level only", "[unit][hiscores]") {
  AgentBoard board;
  board.kind = AgentBoardKind::Skill;
  board.skill = Skill::Speed;
  CHECK(boardScore(levels(50, 90, 33, 200), board) == std::optional<int>{33});
  CHECK_FALSE(boardScore(levels(50, 90, std::nullopt, 200), board).has_value());   // Speed unranked: off this board
  board.skill = Skill::Coding;
  CHECK(boardScore(levels(50, 90, std::nullopt, 200), board) == std::optional<int>{90});
}

TEST_CASE("the Total board needs at least one ranked Skill, and counts unranked ones as 1", "[unit][hiscores]") {
  AgentBoard board;
  board.kind = AgentBoardKind::Total;
  CHECK(boardScore(levels(std::nullopt, 40, std::nullopt, 49), board) == std::optional<int>{49});
  CHECK(boardScore(levels(60, std::nullopt, std::nullopt, 10), board) == std::nullopt);   // nothing ranked: not a floor of 10
}

TEST_CASE("board names read well", "[unit][hiscores]") {
  AgentBoard board;
  CHECK(std::string(boardName(board)) == "Overall");
  board.kind = AgentBoardKind::Total;
  CHECK(std::string(boardName(board)) == "Total level");
  board.kind = AgentBoardKind::Skill;
  board.skill = Skill::ToolUse;
  CHECK(std::string(boardName(board)) == "Tool Use");
}

TEST_CASE("searching by name finds the first match in display order", "[unit][hiscores]") {
  const std::vector<std::string> names{"Mossbeard", "Quillwright", "Tokenmancer", "Lady Lambda", "Mosswalker"};
  CHECK(findByName(names, "quillwright") == std::optional<std::size_t>{1});
  CHECK(findByName(names, "LADY") == std::optional<std::size_t>{3});
  CHECK(findByName(names, "  tokenmancer  ") == std::optional<std::size_t>{2});   // trimmed
  CHECK_FALSE(findByName(names, "nobody").has_value());
}

TEST_CASE("a whole-name match beats a prefix, which beats a substring", "[unit][hiscores]") {
  const std::vector<std::string> names{"Mossy", "Moss Beard", "Moss", "Amoss"};
  CHECK(findByName(names, "moss") == std::optional<std::size_t>{2});     // the exact name, though later
  const std::vector<std::string> noExact{"Amoss", "Mossbeard", "Mossy"};
  CHECK(findByName(noExact, "moss") == std::optional<std::size_t>{1});   // a prefix beats an earlier substring
  CHECK(findByName(noExact, "oss") == std::optional<std::size_t>{0});    // only substrings: the first
}

TEST_CASE("a blank search matches nothing, and an empty list matches nothing", "[unit][hiscores]") {
  const std::vector<std::string> names{"Mossbeard"};
  CHECK_FALSE(findByName(names, "").has_value());
  CHECK_FALSE(findByName(names, "   ").has_value());
  CHECK_FALSE(findByName({}, "moss").has_value());
}
