#include <catch2/catch_test_macros.hpp>

#include <string>

#include "domain/compare.hpp"

using namespace as::domain;

namespace {
CompareColumn col(const std::string& name, std::optional<int> overall, std::optional<int> coding,
                  std::optional<int> speed, int total) {
  CompareColumn c;
  c.name = name;
  c.levels.overall = overall;
  c.levels.skills[0] = coding;
  c.levels.skills[8] = speed;
  c.levels.total = total;
  return c;
}
const CompareRow& row(const Comparison& c, const std::string& label) {
  for (const auto& r : c.rows) {
    if (r.label == label) return r;
  }
  FAIL("no row " << label);
  return c.rows.front();
}
}  // namespace

TEST_CASE("each row marks the highest ranked level", "[unit][compare]") {
  const std::vector<CompareColumn> cols{col("A", 50, 60, 30, 100), col("B", 70, 80, 90, 200), col("C", 60, 70, 10, 150)};
  const auto c = compare(cols, std::nullopt);
  CHECK(row(c, "Coding").best == std::vector<bool>{false, true, false});
  CHECK(row(c, "Speed").best == std::vector<bool>{false, true, false});
  CHECK(row(c, "Overall").best == std::vector<bool>{false, true, false});
  CHECK(row(c, "Total level").best == std::vector<bool>{false, true, false});
  CHECK(c.rows.size() == 12);   // Overall + ten Skills + Total level
  CHECK(c.rows[1].label == "Coding");
  CHECK(c.rows[10].label == "Value");
}

TEST_CASE("ties highlight every tied column", "[unit][compare]") {
  const std::vector<CompareColumn> cols{col("A", 50, 80, 30, 100), col("B", 70, 80, 90, 100), col("C", 60, 80, 90, 100)};
  const auto c = compare(cols, std::nullopt);
  CHECK(row(c, "Coding").best == std::vector<bool>{true, true, true});
  CHECK(row(c, "Speed").best == std::vector<bool>{false, true, true});
}

TEST_CASE("an unranked cell is never best, and an all-unranked row has no best", "[unit][compare]") {
  const std::vector<CompareColumn> cols{col("A", std::nullopt, 12, std::nullopt, 100), col("B", std::nullopt, std::nullopt, std::nullopt, 90)};
  const auto c = compare(cols, std::nullopt);
  CHECK(row(c, "Coding").best == std::vector<bool>{true, false});   // a low ranked level still beats unranked
  CHECK(row(c, "Speed").best == std::vector<bool>{false, false});
  CHECK(row(c, "Overall").best == std::vector<bool>{false, false});
  CHECK_FALSE(row(c, "Coding").levels[1].has_value());
}

TEST_CASE("the scope label names the Quest Type", "[unit][compare]") {
  const std::vector<CompareColumn> cols{col("A", 1, 1, 1, 1), col("B", 1, 1, 1, 1)};
  CHECK(compare(cols, std::nullopt).scopeLabel == "All");
  CHECK(compare(cols, QuestType::Debugging).scopeLabel == "Debugging");
}

TEST_CASE("the clipboard text is an aligned plain table", "[unit][compare]") {
  const std::vector<CompareColumn> cols{col("Gryphon", 76, 71, std::nullopt, 759), col("Cairn", 80, 71, 99, 673)};
  const auto text = toPlainText(compare(cols, QuestType::Coding));
  CHECK(text.rfind("Proving Grounds - Quest Type: Coding\n", 0) == 0);
  CHECK(text.find("Skill       | Gryphon | Cairn\n") != std::string::npos);
  CHECK(text.find("Coding      | 71*     | 71*  \n") != std::string::npos);
  CHECK(text.find("Speed       | -       | 99*  \n") != std::string::npos);
  CHECK(text.find("Total level | 759*    | 673  \n") != std::string::npos);
  CHECK(text.find("Overall     | 76      | 80*  \n") != std::string::npos);
  CHECK(text.find("------------+---------+------\n") != std::string::npos);
  CHECK(text.back() == '\n');
}

TEST_CASE("columns line up by code points for non-ASCII names", "[unit][compare]") {
  const std::vector<CompareColumn> cols{col("Caf\xC3\xA9", 10, 10, 10, 10), col("Bar", 10, 10, 10, 10)};
  const auto text = toPlainText(compare(cols, std::nullopt));
  CHECK(text.find("Skill       | Caf\xC3\xA9 | Bar\n") != std::string::npos);
  CHECK(text.find("Coding      | 10*  | 10*\n") != std::string::npos);   // "Caf\xC3\xA9" is 4 code points, not 5 bytes
}
