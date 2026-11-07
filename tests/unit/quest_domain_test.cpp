#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "domain/ranking.hpp"
#include "domain/ratings.hpp"
#include "domain/xp.hpp"

using namespace as::domain;

// --- XP table ------------------------------------------------------------------------------------------------

TEST_CASE("the XP table matches the exact integer values on every platform", "[unit][xp]") {
  // floor(40 * (L-1)^1.75), computed with exact integer arithmetic (the 4th root of 40^4 * (L-1)^7), so a
  // platform whose pow() rounds differently shows up here instead of as a wrong level.
  static const std::int64_t GOLDEN[] = {
    0LL,
    0LL, 40LL, 134LL, 273LL, 452LL, 668LL,
    920LL, 1204LL, 1522LL, 1870LL, 2249LL, 2657LL,
    3094LL, 3560LL, 4053LL, 4573LL, 5120LL, 5693LL,
    6291LL, 6916LL, 7565LL, 8240LL, 8939LL, 9662LL,
    10409LL, 11180LL, 11974LL, 12792LL, 13632LL, 14496LL,
    15382LL, 16290LL, 17221LL, 18174LL, 19149LL, 20145LL,
    21163LL, 22203LL, 23263LL, 24345LL, 25448LL, 26572LL,
    27717LL, 28882LL, 30067LL, 31273LL, 32500LL, 33746LL,
    35013LL, 36299LL, 37606LL, 38932LL, 40277LL, 41643LL,
    43027LL, 44431LL, 45855LL, 47297LL, 48759LL, 50240LL,
    51739LL, 53258LL, 54795LL, 56351LL, 57926LL, 59519LL,
    61131LL, 62761LL, 64409LL, 66076LL, 67761LL, 69464LL,
    71185LL, 72924LL, 74681LL, 76456LL, 78249LL, 80060LL,
    81888LL, 83735LL, 85598LL, 87480LL, 89378LL, 91294LL,
    93228LL, 95179LL, 97147LL, 99133LL, 101135LL, 103155LL,
    105192LL, 107246LL, 109317LL, 111405LL, 113509LL, 115631LL,
    117769LL, 119925LL, 122097LL,
  };
  const auto& t = xpTable();
  REQUIRE(std::size(GOLDEN) == 100u);
  for (int level = 1; level <= 99; ++level) {
    INFO("level " << level);
    CHECK(t[static_cast<std::size_t>(level)] == GOLDEN[level]);
  }
}

TEST_CASE("well-known XP landmarks", "[unit][xp]") {
  CHECK(xpTable()[1] == 0);
  CHECK(xpTable()[2] == 40);
  CHECK(xpTable()[14] == 3560);
  CHECK(xpTable()[99] == 122097);
}

TEST_CASE("levelForXp lands exactly on the boundaries", "[unit][xp]") {
  CHECK(levelForXp(0) == 1);
  CHECK(levelForXp(39) == 1);
  CHECK(levelForXp(40) == 2);
  CHECK(levelForXp(133) == 2);
  CHECK(levelForXp(134) == 3);
  CHECK(levelForXp(122096) == 98);
  CHECK(levelForXp(122097) == 99);
  CHECK(levelForXp(10'000'000) == 99);
  CHECK(levelForXp(-50) == 1);
}

TEST_CASE("a new Guest starts at Questing level 13 and one Quest Log from level 14", "[unit][xp]") {
  CHECK(guestSeedXp() == 3460);
  CHECK(levelForXp(guestSeedXp()) == 13);
  CHECK(levelForXp(guestSeedXp() + XP_REWARDS.questLog) == 14);   // the first Quest Log triggers a level-up
  CHECK(levelForXp(guestSeedXp() + XP_REWARDS.note) == 13);
}

TEST_CASE("rewards match the spec", "[unit][xp]") {
  CHECK(XP_REWARDS.questLog == 250);
  CHECK(XP_REWARDS.note == 50);
  CHECK(XP_REWARDS.helpfulReceived == 25);
  CHECK(XP_REWARDS.agentApproved == 500);
  CHECK(HELPFUL_DAILY_CAP == 250);
  CHECK(std::string(xpReasonToDb(XpReason::HelpfulReceived)) == "HELPFUL_RECEIVED");
  CHECK(std::string(xpReasonToDb(XpReason::Seed)) == "SEED");
}

TEST_CASE("helpful XP stops at the daily cap", "[unit][xp]") {
  CHECK(helpfulXpAllowed(0) == 25);
  CHECK(helpfulXpAllowed(200) == 25);
  CHECK(helpfulXpAllowed(225) == 25);   // 225 + 25 reaches the cap exactly: allowed
  CHECK(helpfulXpAllowed(226) == 0);    // 226 + 25 would pass it
  CHECK(helpfulXpAllowed(250) == 0);
  CHECK(helpfulXpAllowed(1000) == 0);
  CHECK(helpfulXpAllowed(-5) == 25);
}

TEST_CASE("level-up unlocks follow the catalog and skip what is owned", "[unit][xp]") {
  OwnedSet owned = defaultOwnedSet();
  const auto at1 = newUnlocks(owned, 1);
  const auto at13 = newUnlocks(owned, 13);
  CHECK(at13.size() >= at1.size());
  CHECK(!at13.empty());
  for (const auto& id : at13) owned.insert(id);
  CHECK(newUnlocks(owned, 13).empty());
}

// --- Ranking --------------------------------------------------------------------------------------------------

namespace {
struct Row {
  std::string name;
  int score;
};
std::vector<std::pair<std::string, int>> ranked(std::vector<Row> rows) {
  auto out = competitionRank(std::move(rows), [](const Row& r) { return r.score; }, [](const Row& r) { return r.name; });
  std::vector<std::pair<std::string, int>> flat;
  for (auto& [row, rank] : out) flat.emplace_back(row.name, rank);
  return flat;
}
}  // namespace

TEST_CASE("equal scores share a rank and the next rank skips", "[unit][ranking]") {
  const auto r = ranked({{"d", 10}, {"a", 50}, {"c", 30}, {"b", 30}});
  const std::vector<std::pair<std::string, int>> expected{{"a", 1}, {"b", 2}, {"c", 2}, {"d", 4}};
  CHECK(r == expected);
}

TEST_CASE("ties are ordered by name ignoring case", "[unit][ranking]") {
  const auto r = ranked({{"mira", 5}, {"Zed", 5}, {"alba", 5}, {"Bo", 5}});
  const std::vector<std::pair<std::string, int>> expected{{"alba", 1}, {"Bo", 1}, {"mira", 1}, {"Zed", 1}};
  CHECK(r == expected);
}

TEST_CASE("ranking nothing, or one row", "[unit][ranking]") {
  CHECK(ranked({}).empty());
  const std::vector<std::pair<std::string, int>> one{{"solo", 1}};
  CHECK(ranked({{"solo", 7}}) == one);
}

// --- Rating deltas ----------------------------------------------------------------------------------------------

namespace {
RatingSet set(QuestType qt, int overall, std::map<Skill, int> skills = {}, bool excluded = false) {
  return RatingSet{qt, overall, std::move(skills), excluded};
}
}  // namespace

TEST_CASE("a new Quest Log adds its overall and each Skill rating once", "[unit][ratings]") {
  const auto d = ratingDeltas(std::nullopt, set(QuestType::Coding, 4, {{Skill::Speed, 5}, {Skill::Coding, 3}}));
  REQUIRE(d.size() == 3);
  CHECK(d[0] == Delta{Overall{}, QuestType::Coding, 4, 1});
  CHECK(d[1] == Delta{Skill::Coding, QuestType::Coding, 3, 1});   // Skills in display order
  CHECK(d[2] == Delta{Skill::Speed, QuestType::Coding, 5, 1});
}

TEST_CASE("deleting a Quest Log takes its ratings back out", "[unit][ratings]") {
  const auto d = ratingDeltas(set(QuestType::Coding, 4, {{Skill::Speed, 5}}), std::nullopt);
  REQUIRE(d.size() == 2);
  CHECK(d[0] == Delta{Overall{}, QuestType::Coding, -4, -1});
  CHECK(d[1] == Delta{Skill::Speed, QuestType::Coding, -5, -1});
}

TEST_CASE("changing a rating moves the sum but not the count", "[unit][ratings]") {
  const auto d = ratingDeltas(set(QuestType::Coding, 4, {{Skill::Speed, 5}}), set(QuestType::Coding, 2, {{Skill::Speed, 5}}));
  REQUIRE(d.size() == 1);                                          // the unchanged Speed rating has no delta
  CHECK(d[0] == Delta{Overall{}, QuestType::Coding, -2, 0});
}

TEST_CASE("adding and dropping a Skill rating changes that Skill's count", "[unit][ratings]") {
  const auto added = ratingDeltas(set(QuestType::Data, 3), set(QuestType::Data, 3, {{Skill::Value, 4}}));
  REQUIRE(added.size() == 1);
  CHECK(added[0] == Delta{Skill::Value, QuestType::Data, 4, 1});
  const auto dropped = ratingDeltas(set(QuestType::Data, 3, {{Skill::Value, 4}}), set(QuestType::Data, 3));
  REQUIRE(dropped.size() == 1);
  CHECK(dropped[0] == Delta{Skill::Value, QuestType::Data, -4, -1});
}

TEST_CASE("changing the Quest Type moves the ratings between types", "[unit][ratings]") {
  const auto d = ratingDeltas(set(QuestType::Coding, 4), set(QuestType::Writing, 4));
  REQUIRE(d.size() == 2);
  CHECK(d[0] == Delta{Overall{}, QuestType::Coding, -4, -1});
  CHECK(d[1] == Delta{Overall{}, QuestType::Writing, 4, 1});
}

TEST_CASE("an excluded Quest Log counts as absent", "[unit][ratings]") {
  const auto live = set(QuestType::Coding, 4, {{Skill::Speed, 5}});
  auto gone = live;
  gone.excluded = true;
  CHECK(ratingDeltas(live, gone) == ratingDeltas(live, std::nullopt));
  CHECK(ratingDeltas(gone, live) == ratingDeltas(std::nullopt, live));
  CHECK(ratingDeltas(gone, gone).empty());
  CHECK(ratingDeltas(std::nullopt, gone).empty());
}

TEST_CASE("nothing changing produces no deltas", "[unit][ratings]") {
  const auto s = set(QuestType::Coding, 4, {{Skill::Speed, 5}});
  CHECK(ratingDeltas(s, s).empty());
  CHECK(ratingDeltas(std::nullopt, std::nullopt).empty());
}

TEST_CASE("summing deltas merges the same aggregate and drops cancelled ones", "[unit][ratings]") {
  const auto a = set(QuestType::Coding, 4);
  const auto b = set(QuestType::Coding, 2);
  const std::vector<std::vector<Delta>> lists{ratingDeltas(std::nullopt, a), ratingDeltas(a, b), ratingDeltas(b, std::nullopt)};
  CHECK(sumDeltas(lists).empty());   // created, edited, deleted: back to nothing
  const std::vector<std::vector<Delta>> two{ratingDeltas(std::nullopt, a), ratingDeltas(std::nullopt, b)};
  const auto sum = sumDeltas(two);
  REQUIRE(sum.size() == 1);
  CHECK(sum[0] == Delta{Overall{}, QuestType::Coding, 6, 2});
}

// --- Validation --------------------------------------------------------------------------------------------------

namespace {
QuestLogInput good() {
  QuestLogInput in;
  in.agentId = "seed:agent:gryphon";
  in.questType = QuestType::Coding;
  in.overall = 4;
  in.skills = {{Skill::Coding, 5}, {Skill::Speed, 3}};
  in.title = "A solid helper";
  in.body = "It wrote working code on the first try.\nThe explanations were clear too.";
  in.recommend = true;
  in.pros = {"Accurate", "Fast"};
  in.cons = {"Expensive"};
  return in;
}
bool hasError(const ValidationErrors& e, const std::string& field) {
  for (const auto& x : e) {
    if (x.field == field) return true;
  }
  return false;
}
}  // namespace

TEST_CASE("a good Quest Log input is accepted unchanged", "[unit][ratings]") {
  const auto r = validateQuestLogInput(good());
  REQUIRE(r.has_value());
  CHECK(*r == good());
}

TEST_CASE("text is trimmed, CRLF becomes LF, and inner line breaks are kept", "[unit][ratings]") {
  auto in = good();
  in.version = "  v1.2  ";
  in.title = "   Nice and tidy   ";
  in.body = "  Line one is here.\r\nLine two is here too.  \n";
  const auto r = validateQuestLogInput(in);
  REQUIRE(r.has_value());
  CHECK(r->version == "v1.2");
  CHECK(r->title == "Nice and tidy");
  CHECK(r->body == "Line one is here.\nLine two is here too.");
}

TEST_CASE("every problem is reported, not just the first", "[unit][ratings]") {
  QuestLogInput in;   // agent empty, overall 3 is fine, title/body empty
  const auto r = validateQuestLogInput(in);
  REQUIRE_FALSE(r.has_value());
  CHECK(hasError(r.error(), "agent"));
  CHECK(hasError(r.error(), "title"));
  CHECK(hasError(r.error(), "body"));
  CHECK_FALSE(hasError(r.error(), "overall"));
}

TEST_CASE("ratings must be 1 to 5", "[unit][ratings]") {
  auto in = good();
  in.overall = 0;
  CHECK(hasError(validateQuestLogInput(in).error(), "overall"));
  in.overall = 6;
  CHECK(hasError(validateQuestLogInput(in).error(), "overall"));
  in = good();
  in.skills[Skill::Value] = 6;
  CHECK(hasError(validateQuestLogInput(in).error(), "skills"));
  in.skills[Skill::Value] = 0;
  CHECK(hasError(validateQuestLogInput(in).error(), "skills"));
}

TEST_CASE("length limits are inclusive and counted in characters", "[unit][ratings]") {
  auto in = good();
  in.title = std::string(5, 'x');
  CHECK(validateQuestLogInput(in).has_value());
  in.title = std::string(4, 'x');
  CHECK_FALSE(validateQuestLogInput(in).has_value());
  in.title = std::string(80, 'x');
  CHECK(validateQuestLogInput(in).has_value());
  in.title = std::string(81, 'x');
  CHECK_FALSE(validateQuestLogInput(in).has_value());

  in = good();
  in.body = std::string(20, 'x');
  CHECK(validateQuestLogInput(in).has_value());
  in.body = std::string(19, 'x');
  CHECK_FALSE(validateQuestLogInput(in).has_value());
  in.body = std::string(2000, 'x');
  CHECK(validateQuestLogInput(in).has_value());
  in.body = std::string(2001, 'x');
  CHECK_FALSE(validateQuestLogInput(in).has_value());

  in = good();
  in.version = std::string(40, 'v');
  CHECK(validateQuestLogInput(in).has_value());
  in.version = std::string(41, 'v');
  CHECK(hasError(validateQuestLogInput(in).error(), "version"));

  in = good();
  std::string twoByte;
  for (int i = 0; i < 40; ++i) twoByte += "\xC3\xA9";   // 40 characters, 80 bytes
  in.version = twoByte;
  CHECK(validateQuestLogInput(in).has_value());
}

TEST_CASE("control characters are refused except line breaks in the body", "[unit][ratings]") {
  auto in = good();
  in.title = "tab\there";
  CHECK(hasError(validateQuestLogInput(in).error(), "title"));
  in = good();
  in.title = "line\nbreak";
  CHECK(hasError(validateQuestLogInput(in).error(), "title"));
  in = good();
  in.body = "Bell\a character in a story that is long enough.";
  CHECK(hasError(validateQuestLogInput(in).error(), "body"));
  in = good();
  in.body = "Invalid UTF-8 \xC3 in a story that is long enough.";
  CHECK(hasError(validateQuestLogInput(in).error(), "body"));
}

TEST_CASE("tags come from the fixed lists, at most three each, no repeats", "[unit][ratings]") {
  auto in = good();
  in.pros = {"Accurate", "Fast", "Creative", "Great value"};
  CHECK(hasError(validateQuestLogInput(in).error(), "pros"));
  in.pros = {"Accurate", "Accurate"};
  CHECK(hasError(validateQuestLogInput(in).error(), "pros"));
  in.pros = {"Slow"};   // a con in the pros list
  CHECK(hasError(validateQuestLogInput(in).error(), "pros"));
  in.pros = {"Made up"};
  CHECK(hasError(validateQuestLogInput(in).error(), "pros"));
  in = good();
  in.cons = {"Accurate"};
  CHECK(hasError(validateQuestLogInput(in).error(), "cons"));
  in.cons = {"Slow", "Expensive", "Hallucinated"};
  CHECK(validateQuestLogInput(in).has_value());
}

TEST_CASE("the pro and con lists share no tag, so a stored tag has one side", "[unit][ratings]") {
  for (const auto p : PRO_TAGS) {
    for (const auto c : CON_TAGS) CHECK(p != c);
  }
}

// --- Storage forms ------------------------------------------------------------------------------------------------

TEST_CASE("ratings and tags round-trip through their columns", "[unit][ratings]") {
  const auto in = good();
  CHECK(ratingsFromJson(ratingsToJson(in.skills)) == in.skills);
  std::vector<std::string> pros, cons;
  tagsFromJson(tagsToJson(in.pros, in.cons), pros, cons);
  CHECK(pros == in.pros);
  CHECK(cons == in.cons);
  CHECK(ratingsToJson({}) == "{}");
  CHECK(tagsToJson({}, {}) == "[]");
}

TEST_CASE("damaged stored ratings and tags are read leniently", "[unit][ratings]") {
  CHECK(ratingsFromJson("not json").empty());
  CHECK(ratingsFromJson("[1,2]").empty());
  const auto r = ratingsFromJson(R"({"CODING":4,"CODING_X":3,"SPEED":9,"VALUE":"5","OVERALL":2})");
  CHECK(r == std::map<Skill, Stars>{{Skill::Coding, 4}});   // unknown, out-of-range, non-number and OVERALL entries dropped
  std::vector<std::string> pros{"x"}, cons{"x"};
  tagsFromJson(R"(["Accurate","Nope",7,"Slow"])", pros, cons);
  CHECK(pros == std::vector<std::string>{"Accurate"});
  CHECK(cons == std::vector<std::string>{"Slow"});
  tagsFromJson("{}", pros, cons);
  CHECK(pros.empty());
  CHECK(cons.empty());
}

TEST_CASE("the whole input round-trips through JSON, and bad JSON is refused", "[unit][ratings]") {
  const auto in = good();
  const auto back = questLogInputFromJson(questLogInputToJson(in));
  REQUIRE(back.has_value());
  CHECK(*back == in);
  CHECK_FALSE(questLogInputFromJson("not json").has_value());
  CHECK_FALSE(questLogInputFromJson("{}").has_value());
  CHECK_FALSE(questLogInputFromJson("[]").has_value());
  auto text = questLogInputToJson(in);
  text.replace(text.find("CODING"), 6, "NOPE");   // the quest type is "CODING", the skills key comes later
  CHECK_FALSE(questLogInputFromJson(text).has_value());
}

TEST_CASE("the rating set of an input is its overall and skills", "[unit][ratings]") {
  const auto in = good();
  const auto s = ratingSetOf(in);
  CHECK(s.questType == QuestType::Coding);
  CHECK(s.overall == 4);
  CHECK(s.skills == in.skills);
  CHECK_FALSE(s.excluded);
}
