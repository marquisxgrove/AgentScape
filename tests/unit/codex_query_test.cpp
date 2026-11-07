#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "domain/codex_query.hpp"

using namespace as::domain;

namespace {
Candidate make(std::string name, std::string vendor, AgentKind kind, int total, std::int64_t logs,
               std::optional<int> coding = std::nullopt) {
  Candidate c;
  c.name = std::move(name);
  c.vendor = std::move(vendor);
  c.kind = kind;
  c.levels.total = total;
  c.levels.overallCount = logs;
  c.levels.skills[0] = coding;
  return c;
}

std::vector<Candidate> sample() {
  return {
      make("Claude", "Anthropic", AgentKind::Model, 500, 40, 80),
      make("GPT", "OpenAI", AgentKind::Model, 520, 55, 70),
      make("ChatGPT", "OpenAI", AgentKind::Tool, 480, 90, std::nullopt),
      make("Kiro", "AWS", AgentKind::Tool, 500, 12, 90),
      make("Cursor", "Anysphere", AgentKind::Tool, 410, 30, 60),
      make("Llama", "Meta", AgentKind::Model, 410, 8, std::nullopt),
  };
}

std::vector<std::string> names(const std::vector<Candidate>& c, const std::vector<std::size_t>& idx) {
  std::vector<std::string> out;
  for (const auto i : idx) out.push_back(c[i].name);
  return out;
}
}  // namespace

TEST_CASE("the default list is highest total level first, ties A to Z", "[unit][codex-query]") {
  const auto c = sample();
  CHECK(names(c, selectAndSort(c, {})) == std::vector<std::string>{"GPT", "Claude", "Kiro", "ChatGPT", "Cursor", "Llama"});
}

TEST_CASE("search is case-insensitive over name and vendor, and every word must match", "[unit][codex-query]") {
  const auto c = sample();
  ListQuery q;
  q.text = "OPENai";
  CHECK(names(c, selectAndSort(c, q)) == std::vector<std::string>{"GPT", "ChatGPT"});   // by vendor
  q.text = "gpt";
  CHECK(selectAndSort(c, q).size() == 2);                                                // by name, substring
  q.text = "  openai   chat ";
  CHECK(names(c, selectAndSort(c, q)) == std::vector<std::string>{"ChatGPT"});           // name AND vendor words
  q.text = "zzz";
  CHECK(selectAndSort(c, q).empty());
  q.text = "";
  CHECK(selectAndSort(c, q).size() == c.size());
}

TEST_CASE("kind and vendor filters combine with search", "[unit][codex-query]") {
  const auto c = sample();
  ListQuery q;
  q.kind = AgentKind::Tool;
  CHECK(selectAndSort(c, q).size() == 3);
  q.vendor = "openai";                                   // case-insensitive exact match
  CHECK(names(c, selectAndSort(c, q)) == std::vector<std::string>{"ChatGPT"});
  q.vendor = "open";                                     // exact, not substring
  CHECK(selectAndSort(c, q).empty());
}

TEST_CASE("sorting by name, quest logs, and each direction", "[unit][codex-query]") {
  const auto c = sample();
  ListQuery q;
  q.sort = SortKey::Name;
  CHECK(names(c, selectAndSort(c, q)) == std::vector<std::string>{"ChatGPT", "Claude", "Cursor", "GPT", "Kiro", "Llama"});
  q.reversed = true;
  CHECK(names(c, selectAndSort(c, q)).front() == "Llama");
  q = {};
  q.sort = SortKey::QuestLogs;
  CHECK(names(c, selectAndSort(c, q)).front() == "ChatGPT");     // most Quest Logs first
  q.reversed = true;
  CHECK(names(c, selectAndSort(c, q)).front() == "Llama");       // fewest first
}

TEST_CASE("sorting by a Skill puts unranked last in either direction", "[unit][codex-query]") {
  const auto c = sample();
  ListQuery q;
  q.sort = SortKey::Skill;
  q.skill = Skill::Coding;
  CHECK(names(c, selectAndSort(c, q)) == std::vector<std::string>{"Kiro", "Claude", "GPT", "Cursor", "ChatGPT", "Llama"});
  q.reversed = true;                                              // lowest first...
  CHECK(names(c, selectAndSort(c, q)) == std::vector<std::string>{"Cursor", "GPT", "Claude", "Kiro", "ChatGPT", "Llama"});
  // ...but the two unranked Agents (A to Z) still come last.
}

TEST_CASE("pagination clamps and always has at least one page", "[unit][codex-query]") {
  auto s = paginate(45, 0);
  CHECK((s.first == 0 && s.count == 20 && s.pages == 3 && s.page == 0));
  s = paginate(45, 2);
  CHECK((s.first == 40 && s.count == 5 && s.page == 2));
  s = paginate(45, 99);                                           // past the end: the last page
  CHECK((s.page == 2 && s.first == 40 && s.count == 5));
  s = paginate(40, 1);
  CHECK((s.pages == 2 && s.count == 20));                         // exact multiple: no empty third page
  s = paginate(0, 5);
  CHECK((s.pages == 1 && s.page == 0 && s.count == 0));
  CHECK(CODEX_PAGE_SIZE == 20);
}

TEST_CASE("vendors are distinct and sorted without regard to case", "[unit][codex-query]") {
  auto c = sample();
  c.push_back(make("Other", "openai", AgentKind::Model, 1, 1));   // same vendor, different case
  CHECK(distinctVendors(c) == std::vector<std::string>{"Anthropic", "Anysphere", "AWS", "Meta", "OpenAI"});
}
