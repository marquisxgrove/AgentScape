#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "domain/rate_limit.hpp"
#include "domain/skills.hpp"
#include "domain/slug.hpp"
#include "domain/voting.hpp"
#include "generators/rc_catch.hpp"

using namespace as::domain;

namespace {
constexpr int SCORING_RUNS = 1000;  // testing.md: scoring and aggregation math uses 1000 runs

// RapidCheck hands us arbitrary ints; fold them into valid star ratings.
std::vector<int> toRatings(const std::vector<int>& raw) {
  std::vector<int> r;
  r.reserve(raw.size());
  for (const int v : raw) r.push_back(1 + std::abs(v % 5));
  return r;
}
}  // namespace

// Validates: agent-codex Requirements 3.2, 3.3
TEST_CASE("Property 1: Level bounds and ranking threshold", "[property][scoring][agent-codex]") {
  as::testing::checkPropertyRuns("unranked below 3 ratings, otherwise within [1, 99]", SCORING_RUNS,
                                 [](const std::vector<int>& raw) {
                                   const auto ratings = toRatings(raw);
                                   const auto level = skillLevel(aggOf(ratings));
                                   if (ratings.size() < 3) {
                                     RC_ASSERT(!level.has_value());
                                   } else {
                                     RC_ASSERT(level.has_value());
                                     RC_ASSERT(*level >= 1);
                                     RC_ASSERT(*level <= 99);
                                   }
                                 });
}

// Validates: agent-codex Requirement 3.1
TEST_CASE("Property 2: Order independence", "[property][scoring][agent-codex]") {
  as::testing::checkPropertyRuns("any permutation of the ratings gives the same level", SCORING_RUNS,
                                 [](const std::vector<int>& raw, const std::uint32_t seed) {
                                   const auto ratings = toRatings(raw);
                                   auto shuffled = ratings;
                                   std::mt19937 gen(seed);
                                   std::shuffle(shuffled.begin(), shuffled.end(), gen);
                                   RC_ASSERT(skillLevel(aggOf(ratings)) == skillLevel(aggOf(shuffled)));
                                   // Folding the ratings in one at a time, in shuffled order, agrees too.
                                   Agg running;
                                   for (const int r : shuffled) {
                                     const Agg one{r, 1};
                                     const Agg parts[] = {running, one};
                                     running = mergeAgg(parts);
                                   }
                                   RC_ASSERT(running == aggOf(ratings));
                                 });
}

// Validates: agent-codex Requirement 3.1
TEST_CASE("Property 3: Monotonicity", "[property][scoring][agent-codex]") {
  as::testing::checkPropertyRuns("a 5 never lowers the level and a 1 never raises it", SCORING_RUNS,
                                 [](const std::vector<int>& raw) {
                                   auto ratings = toRatings(raw);
                                   RC_PRE(ratings.size() >= 3);
                                   const int base = *skillLevel(aggOf(ratings));
                                   auto up = ratings;
                                   up.push_back(5);
                                   auto down = ratings;
                                   down.push_back(1);
                                   RC_ASSERT(*skillLevel(aggOf(up)) >= base);
                                   RC_ASSERT(*skillLevel(aggOf(down)) <= base);
                                 });
}

// Validates: agent-codex Requirements 3.4, 3.5
TEST_CASE("Property 4: Scope additivity", "[property][scoring][agent-codex]") {
  as::testing::checkPropertyRuns("merging per-quest-type aggregates equals the all-types computation", SCORING_RUNS,
                                 [](const std::vector<int>& raw, const std::vector<int>& typeRaw) {
                                   const auto ratings = toRatings(raw);
                                   std::map<QuestType, std::vector<int>> byType;
                                   for (std::size_t i = 0; i < ratings.size(); ++i) {
                                     const int t = typeRaw.empty() ? 0 : std::abs(typeRaw[i % typeRaw.size()]) % 8;
                                     byType[QUEST_TYPES[static_cast<std::size_t>(t)]].push_back(ratings[i]);
                                   }
                                   std::vector<Agg> perType;
                                   for (const auto& [type, rs] : byType) perType.push_back(aggOf(rs));
                                   RC_ASSERT(mergeAgg(perType) == aggOf(ratings));
                                   RC_ASSERT(skillLevel(mergeAgg(perType)) == skillLevel(aggOf(ratings)));
                                 });
}

// Validates: agent-codex Requirement 5.5
TEST_CASE("Property 5: Wilson bounds and ordering", "[property][scoring][agent-codex]") {
  as::testing::checkPropertyRuns("within [0, 1], and non-decreasing in agree votes for a fixed total", SCORING_RUNS,
                                 [](const int rawA, const int rawD) {
                                   const std::int64_t a = std::abs(rawA % 5000);
                                   const std::int64_t d = std::abs(rawD % 5000);
                                   const double w = wilsonLowerBound(a, d);
                                   RC_ASSERT(w >= 0.0);
                                   RC_ASSERT(w <= 1.0);
                                   const std::int64_t n = a + d;
                                   if (n > 0 && a < n) {
                                     // one more agree, one fewer disagree, same total
                                     RC_ASSERT(wilsonLowerBound(a + 1, d - 1) >= w - 1e-12);
                                   }
                                 });
  CHECK(wilsonLowerBound(0, 0) == 0.0);  // no votes -> no score
}

// Validates: agent-codex Requirements 5.3, 5.4
TEST_CASE("Property 6: Vote model", "[property][agent-codex]") {
  as::testing::checkProperty("one vote per user, tally matches the final votes, self-votes change nothing",
                             [](const std::vector<std::pair<int, int>>& script) {
                               const std::string author = "author";
                               VoteMap votes;
                               std::map<std::string, VoteValue> reference;  // the final vote of each non-author user
                               for (const auto& [rawUser, rawKind] : script) {
                                 const int u = std::abs(rawUser % 6);
                                 const std::string user = u == 0 ? author : "user" + std::to_string(u);
                                 VoteOp op{user, "note-1", std::nullopt};
                                 switch (std::abs(rawKind % 3)) {
                                   case 0: op.value = VoteValue::Agree; break;
                                   case 1: op.value = VoteValue::Disagree; break;
                                   default: break;  // retract
                                 }
                                 const VoteMap before = votes;
                                 const auto result = applyVote(votes, op, author);
                                 if (user == author) {
                                   RC_ASSERT(!result.has_value());
                                   RC_ASSERT(result.error() == VoteError::SelfVote);
                                   RC_ASSERT(votes == before);  // state unchanged
                                   continue;
                                 }
                                 RC_ASSERT(result.has_value());
                                 votes = *result;
                                 if (op.value) {
                                   reference[user] = *op.value;
                                 } else {
                                   reference.erase(user);
                                 }
                               }
                               RC_ASSERT(votes.size() == reference.size());  // at most one vote per user
                               Tally expected;
                               for (const auto& [user, v] : reference) {
                                 RC_ASSERT(votes.at(user) == v);
                                 (v == VoteValue::Agree ? expected.agree : expected.disagree)++;
                               }
                               RC_ASSERT(tally(votes) == expected);
                             });
}

// Validates: agent-codex Requirement 4.2
TEST_CASE("Property 7: Slug normalization", "[property][agent-codex]") {
  as::testing::checkProperty("matches ^[a-z0-9]+(-[a-z0-9]+)*$ (or is empty) and is idempotent",
                             [](const std::string& raw) {
                               const std::string slug = slugify(raw);
                               if (!slug.empty()) {
                                 RC_ASSERT(slug.front() != '-');
                                 RC_ASSERT(slug.back() != '-');
                               }
                               for (std::size_t i = 0; i < slug.size(); ++i) {
                                 const char c = slug[i];
                                 RC_ASSERT(((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'));
                                 if (c == '-') RC_ASSERT(slug[i + 1] != '-');  // front/back already excluded
                               }
                               RC_ASSERT(slugify(slug) == slug);
                             });
}

// Validates: agent-codex Requirements 4.3, 5.2
TEST_CASE("Property 8: Rate limit", "[property][agent-codex]") {
  as::testing::checkProperty("no window holds more recorded attempts than the limit",
                             [](const std::vector<int>& gaps, const int rawLimit, const int rawWindow) {
                               using std::chrono::seconds;
                               const int limit = 1 + std::abs(rawLimit % 6);
                               const seconds window{1 + std::abs(rawWindow % 120)};
                               std::vector<std::chrono::sys_seconds> recorded;
                               std::chrono::sys_seconds now{seconds{1'000'000}};
                               for (const int gap : gaps) {
                                 now += seconds{std::abs(gap % 40)};
                                 if (isAllowed(recorded, now, limit, window)) recorded.push_back(now);
                               }
                               for (const auto& end : recorded) {
                                 int inWindow = 0;
                                 for (const auto& t : recorded) {
                                   if (t <= end && end - t < window) ++inWindow;
                                 }
                                 RC_ASSERT(inWindow <= limit);
                               }
                             });
}

#include "domain/codex_query.hpp"

namespace {
std::vector<Candidate> randomCandidates(std::uint64_t seed, std::size_t n) {
  std::mt19937_64 gen(seed);
  static const char* vendors[] = {"Anthropic", "OpenAI", "Google", "Meta", "AWS"};
  std::vector<Candidate> out;
  for (std::size_t i = 0; i < n; ++i) {
    Candidate c;
    c.name = "Agent" + std::to_string(gen() % 30);   // duplicates on purpose: ties
    c.vendor = vendors[gen() % 5];
    c.kind = gen() % 2 ? AgentKind::Tool : AgentKind::Model;
    c.levels.total = 10 + static_cast<int>(gen() % 500);
    c.levels.overallCount = static_cast<std::int64_t>(gen() % 50);
    for (auto& s : c.levels.skills) {
      if (gen() % 3) s = 1 + static_cast<int>(gen() % 99);
    }
    out.push_back(std::move(c));
  }
  return out;
}
}  // namespace

// Validates: agent-codex Requirements 1.1-1.4
TEST_CASE("Property 9: Codex listing is a sorted, filtered permutation", "[property][agent-codex]") {
  as::testing::checkProperty("a permutation of the filtered set, sorted, unranked last, pages cover it once",
                             [](const std::uint64_t seed, const int rawSort, const bool reversed, const int rawSkill) {
                               const auto cands = randomCandidates(seed, 1 + seed % 70);
                               ListQuery q;
                               q.sort = static_cast<SortKey>(std::abs(rawSort) % 4);
                               q.reversed = reversed;
                               q.skill = SKILLS[static_cast<std::size_t>(std::abs(rawSkill) % 10)];
                               q.kind = seed % 3 == 0 ? std::optional<AgentKind>{AgentKind::Tool} : std::nullopt;
                               const auto idx = selectAndSort(cands, q);

                               // Exactly the candidates that pass the filter, each once.
                               std::vector<std::size_t> sorted = idx;
                               std::sort(sorted.begin(), sorted.end());
                               RC_ASSERT(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
                               std::size_t expected = 0;
                               for (const auto& c : cands) expected += (!q.kind || c.kind == *q.kind);
                               RC_ASSERT(idx.size() == expected);

                               // Ordered by the chosen key; unranked Skills last.
                               const auto skillIdx = static_cast<std::size_t>(std::find(SKILLS.begin(), SKILLS.end(), q.skill) - SKILLS.begin());
                               const bool natAsc = q.sort == SortKey::Name;
                               const bool asc = natAsc != q.reversed;
                               bool seenUnranked = false;
                               for (std::size_t i = 0; i + 1 < idx.size(); ++i) {
                                 const auto& a = cands[idx[i]];
                                 const auto& b = cands[idx[i + 1]];
                                 if (q.sort == SortKey::Skill) {
                                   const auto& la = a.levels.skills[skillIdx];
                                   const auto& lb = b.levels.skills[skillIdx];
                                   if (!la) seenUnranked = true;
                                   if (seenUnranked) RC_ASSERT(!lb.has_value());      // nothing ranked after an unranked
                                   if (la && lb) {
                                     const bool inOrder = asc ? *la <= *lb : *la >= *lb;
                                     RC_ASSERT(inOrder);
                                   }
                                 } else if (q.sort == SortKey::Total) {
                                   const bool inOrder = asc ? a.levels.total <= b.levels.total : a.levels.total >= b.levels.total;
                                   RC_ASSERT(inOrder);
                                 } else if (q.sort == SortKey::QuestLogs) {
                                   const bool inOrder = asc ? a.levels.overallCount <= b.levels.overallCount
                                                            : a.levels.overallCount >= b.levels.overallCount;
                                   RC_ASSERT(inOrder);
                                 }
                               }

                               // Walking every page shows each result exactly once.
                               std::size_t shown = 0;
                               const auto pages = paginate(idx.size(), 0).pages;
                               for (std::size_t p = 0; p < pages; ++p) {
                                 const auto s = paginate(idx.size(), p);
                                 RC_ASSERT(s.count <= CODEX_PAGE_SIZE);
                                 shown += s.count;
                               }
                               RC_ASSERT(shown == idx.size());
                             });
}
