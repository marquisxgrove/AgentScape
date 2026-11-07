#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "domain/ranking.hpp"
#include "domain/ratings.hpp"
#include "domain/xp.hpp"
#include "generators/quest_gen.hpp"
#include "generators/rc_catch.hpp"

using namespace as::domain;

namespace {
constexpr int SCORING_RUNS = 1000;   // testing.md: scoring and aggregation math uses 1000 runs

// (Quest Type, stat) -> (sum, count): the shape of agent_skill_stat.
using AggKey = std::pair<int, int>;   // stat: 0 = Overall, 1..10 = Skills
using AggMap = std::map<AggKey, std::pair<std::int64_t, std::int64_t>>;

int statIndexOf(const StatKey& key) {
  return std::holds_alternative<Overall>(key) ? 0 : 1 + static_cast<int>(std::get<Skill>(key));
}

void fold(AggMap& agg, const std::vector<Delta>& deltas) {
  for (const auto& d : deltas) {
    auto& cell = agg[{static_cast<int>(d.questType), statIndexOf(d.skill)}];
    cell.first += d.sumDelta;
    cell.second += d.countDelta;
  }
}

// Aggregates recomputed from scratch over the live, non-excluded Quest Logs.
AggMap recompute(const std::array<std::optional<RatingSet>, 4>& logs) {
  AggMap agg;
  for (const auto& log : logs) {
    if (!log || log->excluded) continue;
    auto add = [&](int stat, int stars) {
      auto& cell = agg[{static_cast<int>(log->questType), stat}];
      cell.first += stars;
      cell.second += 1;
    };
    add(0, log->overall);
    for (const auto& [skill, stars] : log->skills) add(1 + static_cast<int>(skill), stars);
  }
  return agg;
}

// Compares two aggregate maps, treating a missing cell as (0, 0).
bool sameAggregates(const AggMap& a, const AggMap& b) {
  auto get = [](const AggMap& m, const AggKey& k) {
    const auto it = m.find(k);
    return it == m.end() ? std::pair<std::int64_t, std::int64_t>{0, 0} : it->second;
  };
  for (const auto& [k, v] : a) {
    if (get(b, k) != v) return false;
  }
  for (const auto& [k, v] : b) {
    if (get(a, k) != v) return false;
  }
  return true;
}

enum class OpKind { Save, Delete, SetExcluded };
struct Op {
  OpKind kind;
  std::size_t slot;
  RatingSet set;
  bool excluded;
};

rc::Gen<Op> genOp() {
  return rc::gen::exec([]() {
    Op op;
    op.kind = static_cast<OpKind>(*rc::gen::inRange(0, 3));
    op.slot = static_cast<std::size_t>(*rc::gen::inRange(0, 4));
    op.set = *as::testing::genRatingSet();
    op.excluded = *rc::gen::arbitrary<bool>();
    return op;
  });
}
}  // namespace

// Validates: quest-logs Requirements 2.1, 2.2, 2.3
TEST_CASE("Property 1: Incremental aggregates equal recompute", "[property][scoring][quest-logs]") {
  as::testing::checkPropertyRuns("folding ratingDeltas over create/edit/delete/exclude equals a recompute", SCORING_RUNS, [] {
    const auto ops = *rc::gen::container<std::vector<Op>>(genOp());
    std::array<std::optional<RatingSet>, 4> logs;
    AggMap incremental;
    for (const Op& op : ops) {
      const auto before = logs[op.slot];
      switch (op.kind) {
        case OpKind::Save:   // create, or edit what is there
          logs[op.slot] = op.set;
          break;
        case OpKind::Delete:
          logs[op.slot].reset();
          break;
        case OpKind::SetExcluded:
          if (logs[op.slot]) logs[op.slot]->excluded = op.excluded;
          break;
      }
      fold(incremental, ratingDeltas(before, logs[op.slot]));

      RC_ASSERT(sameAggregates(incremental, recompute(logs)));
      // The folded aggregates must always be ones the database CHECK would accept: count >= 0 and
      // count <= sum <= 5 * count.
      for (const auto& [key, cell] : incremental) {
        RC_ASSERT(cell.second >= 0);
        RC_ASSERT(cell.first >= cell.second);
        RC_ASSERT(cell.first <= 5 * cell.second);
      }
    }
  });
}

// Validates: quest-logs Requirement 2.1
TEST_CASE("Property 2: Delta algebra", "[property][scoring][quest-logs]") {
  as::testing::checkPropertyRuns("deltas compose, and no change is no delta", SCORING_RUNS, [] {
    const auto a = *as::testing::genMaybeRatingSet();
    const auto b = *as::testing::genMaybeRatingSet();
    const auto c = *as::testing::genMaybeRatingSet();
    const std::vector<std::vector<Delta>> steps{ratingDeltas(a, b), ratingDeltas(b, c)};
    RC_ASSERT(sumDeltas(steps) == ratingDeltas(a, c));
    RC_ASSERT(ratingDeltas(a, a).empty());
  });
}

// Validates: quest-logs Requirement 5.4
TEST_CASE("Property 3: XP table and level", "[property][scoring][quest-logs]") {
  const auto& t = xpTable();
  REQUIRE(t[1] == 0);
  for (int l = 1; l < MAX_QUESTING_LEVEL; ++l) {
    INFO("level " << l);
    REQUIRE(t[static_cast<std::size_t>(l)] < t[static_cast<std::size_t>(l) + 1]);   // strictly increasing
  }
  as::testing::checkPropertyRuns("levelForXp is the largest level whose requirement is met", SCORING_RUNS, [] {
    const std::int64_t xp = *rc::gen::inRange<std::int64_t>(0, 200000);
    const int level = levelForXp(xp);
    RC_ASSERT(level >= 1);
    RC_ASSERT(level <= MAX_QUESTING_LEVEL);
    RC_ASSERT(xpTable()[static_cast<std::size_t>(level)] <= xp);
    // Evaluated outside RC_ASSERT: its expression capture overloads ||, which loses short-circuiting
    // and would read xpTable()[100] (out of bounds) at the top level.
    const bool belowNext = level == MAX_QUESTING_LEVEL || xp < xpTable()[static_cast<std::size_t>(level) + 1];
    RC_ASSERT(belowNext);
  });
}

// Validates: quest-logs Requirement 5.4
TEST_CASE("Property 4: Level monotonicity", "[property][scoring][quest-logs]") {
  as::testing::checkPropertyRuns("more XP never means a lower level", SCORING_RUNS, [] {
    const std::int64_t x = *rc::gen::inRange<std::int64_t>(0, 200000);
    const std::int64_t y = *rc::gen::inRange<std::int64_t>(x, 200001);
    RC_ASSERT(levelForXp(x) <= levelForXp(y));
  });
}

// Validates: quest-logs Requirements 6.1, 6.3
TEST_CASE("Property 5: Unlocks are monotone and idempotent", "[property][quest-logs]") {
  as::testing::checkProperty("granting what newUnlocks returns leaves nothing new, and higher levels only add", [] {
    const int l1 = *rc::gen::inRange(1, 100);
    const int l2 = *rc::gen::inRange(l1, 100);
    OwnedSet owned = defaultOwnedSet();
    // Start from some arbitrary subset of what level 99 would unlock.
    for (const auto& id : newUnlocks(owned, 99)) {
      if (*rc::gen::arbitrary<bool>()) owned.insert(id);
    }
    const auto first = newUnlocks(owned, l1);
    OwnedSet after = owned;
    after.insert(first.begin(), first.end());
    RC_ASSERT(newUnlocks(after, l1).empty());                       // idempotent

    const OwnedSet none = defaultOwnedSet();
    const auto low = newUnlocks(none, l1);
    const auto high = newUnlocks(none, l2);
    for (const auto& id : low) RC_ASSERT(std::find(high.begin(), high.end(), id) != high.end());   // monotone
  });
}

// Validates: quest-logs Requirement 5.1
TEST_CASE("Property 6: Helpful XP daily cap", "[property][scoring][quest-logs]") {
  as::testing::checkPropertyRuns("a day's helpful XP is min(25 * votes, 250)", SCORING_RUNS, [] {
    const int votes = *rc::gen::inRange(0, 60);
    int total = 0;
    for (int i = 0; i < votes; ++i) {
      const int granted = helpfulXpAllowed(total);
      RC_ASSERT(granted == 0 || granted == XP_REWARDS.helpfulReceived);
      total += granted;
      RC_ASSERT(total <= HELPFUL_DAILY_CAP);
    }
    RC_ASSERT(total == std::min(XP_REWARDS.helpfulReceived * votes, HELPFUL_DAILY_CAP));
  });
}

namespace {
struct RankRow {
  std::string name;
  int score;
};
}  // namespace

// Validates: quest-logs Requirement 7.3
TEST_CASE("Property 7: Competition ranking", "[property][scoring][quest-logs]") {
  as::testing::checkPropertyRuns("equal scores share a rank, and a rank is 1 + the rows strictly above", SCORING_RUNS, [] {
    const auto count = *rc::gen::inRange(0, 30);
    std::vector<RankRow> rows;
    for (int i = 0; i < count; ++i) {
      rows.push_back({*rc::gen::map(rc::gen::inRange(0, 1000), [](int n) { return "n" + std::to_string(n); }),
                      *rc::gen::inRange(0, 6)});   // few distinct scores, so ties are common
    }
    const auto ranked = competitionRank(rows, [](const RankRow& r) { return r.score; }, [](const RankRow& r) { return r.name; });
    RC_ASSERT(ranked.size() == rows.size());

    for (std::size_t i = 0; i < ranked.size(); ++i) {
      const auto& [row, rank] = ranked[i];
      const int above = static_cast<int>(std::count_if(rows.begin(), rows.end(), [&](const RankRow& r) { return r.score > row.score; }));
      RC_ASSERT(rank == 1 + above);
      if (i > 0) {
        RC_ASSERT(ranked[i - 1].second <= rank);                                       // ranks never go down
        RC_ASSERT(ranked[i - 1].first.score >= row.score);                             // best score first
        if (ranked[i - 1].first.score == row.score) {
          RC_ASSERT(ranked[i - 1].second == rank);                                     // equal scores share a rank
        }
      }
    }
  });
}

namespace {
std::string repeated(int n) { return std::string(static_cast<std::size_t>(n), 'a'); }
}  // namespace

// Validates: quest-logs Requirement 1.1
TEST_CASE("Property 8: Input validation", "[property][quest-logs]") {
  as::testing::checkProperty("accepted if and only if every Requirement 1.1 constraint holds", [] {
    QuestLogInput in;
    const bool hasAgent = *rc::gen::inRange(0, 8) != 0;
    in.agentId = hasAgent ? "seed:agent:x" : "";
    const int versionLen = *rc::gen::inRange(0, 50);
    in.version = repeated(versionLen);
    in.questType = QUEST_TYPES[static_cast<std::size_t>(*rc::gen::inRange(0, 8))];
    in.overall = *rc::gen::inRange(0, 7);
    bool skillsOk = true;
    for (const auto skill : SKILLS) {
      if (*rc::gen::inRange(0, 3) != 0) continue;
      const int stars = *rc::gen::inRange(0, 7);
      in.skills[skill] = stars;
      skillsOk = skillsOk && stars >= 1 && stars <= 5;
    }
    const int titleLen = *rc::gen::inRange(0, 100);
    const int bodyLen = *rc::gen::inRange(0, 2100);
    // Surrounding spaces must not count towards the length.
    in.title = "  " + repeated(titleLen) + " ";
    in.body = " " + repeated(bodyLen) + "  ";
    in.recommend = *rc::gen::arbitrary<bool>();

    // Tags: mostly valid picks, sometimes a stranger, a repeat, or too many.
    bool prosOk = true;
    bool consOk = true;
    const auto pickTags = [&](const auto& list, const auto& other, std::vector<std::string>& out, bool& ok) {
      const int n = *rc::gen::inRange(0, 6);
      for (int i = 0; i < n; ++i) {
        const int which = *rc::gen::inRange(0, 10);
        std::string tag;
        if (which == 9) {
          tag = "Not a tag";
        } else if (which == 8) {
          tag = std::string(other[static_cast<std::size_t>(*rc::gen::inRange(0, 8))]);   // from the other side
        } else {
          tag = std::string(list[static_cast<std::size_t>(*rc::gen::inRange(0, 8))]);
        }
        out.push_back(tag);
      }
      ok = out.size() <= 3;
      for (std::size_t i = 0; i < out.size(); ++i) {
        ok = ok && std::find(list.begin(), list.end(), out[i]) != list.end();
        for (std::size_t j = 0; j < i; ++j) ok = ok && out[j] != out[i];
      }
    };
    pickTags(PRO_TAGS, CON_TAGS, in.pros, prosOk);
    pickTags(CON_TAGS, PRO_TAGS, in.cons, consOk);

    const bool expected = hasAgent && versionLen <= 40 && in.overall >= 1 && in.overall <= 5 && skillsOk &&
                          titleLen >= 5 && titleLen <= 80 && bodyLen >= 20 && bodyLen <= 2000 && prosOk && consOk;
    const auto result = validateQuestLogInput(in);
    RC_ASSERT(result.has_value() == expected);
    if (result) {
      // Accepted inputs round-trip through the JSON codec unchanged.
      const auto back = questLogInputFromJson(questLogInputToJson(*result));
      RC_ASSERT(back.has_value());
      RC_ASSERT(*back == *result);
      // And validating what was accepted changes nothing.
      const auto again = validateQuestLogInput(*result);
      RC_ASSERT(again.has_value());
      RC_ASSERT(*again == *result);
    }
  });
}
