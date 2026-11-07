#include <catch2/catch_test_macros.hpp>

#include <map>
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include "app/event_bus.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "domain/ratings.hpp"
#include "domain/xp.hpp"
#include "services/activity_service.hpp"
#include "services/adventurer_service.hpp"
#include "services/agent_service.hpp"
#include "services/keeper_service.hpp"
#include "services/note_service.hpp"
#include "services/profile_service.hpp"
#include "services/quest_service.hpp"
#include "services/xp_service.hpp"

using namespace as;
using namespace as::services;
using domain::QuestType;
using domain::Skill;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};   // 2026-11-04 mid-day UTC
const std::string GRYPHON = "seed:agent:gryphon";
const std::string CAIRN = "seed:agent:cairn";

struct Fixture {
  db::Database db;
  core::FixedClock clock{NOW};
  core::SplitMix64 rng{21};
  app::EventBus bus;
  ActivityService activity;
  AdventurerService adventurers;
  XpService xp;
  QuestService quests;

  std::vector<std::string> ratingEvents;
  std::vector<app::XpChanged> xpEvents;
  std::vector<std::string> unlockEvents;
  std::vector<std::string> feed;
  std::vector<app::Subscription> subs;

  Fixture()
      : db(open()), activity(db, clock, bus), adventurers(db, clock, rng), xp(db, clock, rng, activity),
        quests(db, clock, rng, bus) {
    player("alice", "Alice", true);
    player("bob", "Bobby", true);
    player("carol", "Carol", false);   // no Adventurer
    player("kay", "Kay", true);
    db.exec("UPDATE user SET role = 'KEEPER' WHERE id = 'kay'");
    subs.push_back(app::Subscription{bus, bus.subscribe<app::RatingsChanged>([this](const app::RatingsChanged& e) { ratingEvents.push_back(e.agentId); })});
    subs.push_back(app::Subscription{bus, bus.subscribe<app::XpChanged>([this](const app::XpChanged& e) { xpEvents.push_back(e); })});
    subs.push_back(app::Subscription{bus, bus.subscribe<app::ItemUnlocked>([this](const app::ItemUnlocked& e) { unlockEvents.push_back(e.itemId); })});
    subs.push_back(app::Subscription{bus, bus.subscribe<app::ActivityPosted>([this](const app::ActivityPosted& e) { feed.push_back(e.message.text); })});
  }

  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, NOW).has_value());
    return std::move(*d);
  }

  void player(const std::string& id, const std::string& name, bool withAdventurer) {
    db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('" + id + "', '" + name + "', 'PLAYER', 't', 't')");
    if (withAdventurer) REQUIRE(adventurers.createAdventurer(id, name).has_value());
  }

  std::int64_t scalar(const std::string& sql) {
    auto st = db.prepare(sql);
    REQUIRE(st.step());
    return st.columnInt(0);
  }

  void clearEvents() {
    ratingEvents.clear();
    xpEvents.clear();
    unlockEvents.clear();
    feed.clear();
  }

  static domain::QuestLogInput input(const std::string& agent = GRYPHON, const std::string& version = "") {
    domain::QuestLogInput in;
    in.agentId = agent;
    in.version = version;
    in.questType = QuestType::Coding;
    in.overall = 4;
    in.skills = {{Skill::Coding, 5}, {Skill::Speed, 3}};
    in.title = "A solid helper";
    in.body = "It wrote working code on the first try.\nThe explanations were clear too.";
    in.recommend = true;
    in.pros = {"Accurate"};
    in.cons = {"Expensive"};
    return in;
  }

  // The player-added part of agent_skill_stat, and the same recomputed from the live Quest Logs.
  using Cell = std::tuple<std::string, std::string, std::string>;   // agent, stat, quest type
  std::map<Cell, std::pair<std::int64_t, std::int64_t>> storedAggregates() {
    std::map<Cell, std::pair<std::int64_t, std::int64_t>> out;
    auto st = db.prepare("SELECT agent_id, skill, quest_type, sum - seed_sum, count - seed_count FROM agent_skill_stat");
    while (st.step()) {
      const std::pair<std::int64_t, std::int64_t> v{st.columnInt(3), st.columnInt(4)};
      if (v.first != 0 || v.second != 0) out[{st.columnText(0), st.columnText(1), st.columnText(2)}] = v;
    }
    return out;
  }
  std::map<Cell, std::pair<std::int64_t, std::int64_t>> recomputedAggregates() {
    std::map<Cell, std::pair<std::int64_t, std::int64_t>> out;
    auto st = db.prepare("SELECT agent_id, quest_type, overall, ratings FROM quest_log WHERE excluded = 0");
    while (st.step()) {
      const auto agent = st.columnText(0);
      const auto qt = st.columnText(1);
      auto add = [&](const std::string& stat, std::int64_t stars) {
        auto& cell = out[{agent, stat, qt}];
        cell.first += stars;
        cell.second += 1;
      };
      add("OVERALL", st.columnInt(2));
      for (const auto& [skill, stars] : domain::ratingsFromJson(st.columnText(3))) add(domain::storageName(domain::StatKey{skill}), stars);
    }
    return out;
  }
  void checkInvariants() {
    CHECK(storedAggregates() == recomputedAggregates());
    auto st = db.prepare("SELECT u.id, u.total_xp, COALESCE((SELECT SUM(amount) FROM xp_entry WHERE user_id = u.id), 0) FROM user u");
    while (st.step()) {
      INFO("user " << st.columnText(0));
      CHECK(st.columnInt(1) == st.columnInt(2));   // total_xp is the ledger sum
    }
  }
  std::int64_t totalXp(const std::string& userId) { return scalar("SELECT total_xp FROM user WHERE id = '" + userId + "'"); }
  void setXp(const std::string& userId, std::int64_t xpAmount) {   // a seed ledger row, like a Guest's
    db.exec("INSERT INTO xp_entry(id, user_id, reason, ref_id, amount, created_at) VALUES ('seed-" + userId + "', '" + userId +
            "', 'SEED', 'seed', " + std::to_string(xpAmount) + ", 't')");
    db.exec("UPDATE user SET total_xp = total_xp + " + std::to_string(xpAmount) + " WHERE id = '" + userId + "'");
  }
};

QuestError errorOf(const Result<QuestOutcome, QuestError>& r) {
  REQUIRE_FALSE(r.has_value());
  return r.error();
}
}  // namespace

TEST_CASE("writing a Quest Log saves it, feeds the Codex, pays 250 XP and tells the feed", "[integration][quests]") {
  Fixture f;
  const auto before = f.storedAggregates();
  CHECK(before.empty());
  const auto out = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(out.has_value());
  CHECK(out->xpGained == 250);
  CHECK(f.totalXp("alice") == 250);

  const auto log = f.quests.get(out->questLogId);
  REQUIRE(log.has_value());
  REQUIRE(log->has_value());
  CHECK((*log)->authorName == "Alice");
  CHECK((*log)->agentName == "Gryphon");
  CHECK((*log)->input == Fixture::input());
  CHECK((*log)->snapshot.find("\"name\":\"Alice\"") != std::string::npos);   // the Adventurer, frozen
  CHECK((*log)->helpfulCount == 0);

  const auto agg = f.storedAggregates();
  CHECK(agg.at({GRYPHON, "OVERALL", "CODING"}) == std::pair<std::int64_t, std::int64_t>{4, 1});
  CHECK(agg.at({GRYPHON, "CODING", "CODING"}) == std::pair<std::int64_t, std::int64_t>{5, 1});
  CHECK(agg.at({GRYPHON, "SPEED", "CODING"}) == std::pair<std::int64_t, std::int64_t>{3, 1});
  CHECK(agg.size() == 3);
  f.checkInvariants();

  CHECK(f.ratingEvents == std::vector<std::string>{GRYPHON});
  REQUIRE(f.xpEvents.size() == 1);
  CHECK(f.xpEvents[0].userId == "alice");
  CHECK(f.xpEvents[0].totalXp == 250);
  REQUIRE(f.feed.size() == 1);
  CHECK(f.feed[0] == "Alice completed a Coding quest with Gryphon (4/5).");
  CHECK(f.scalar("SELECT COUNT(*) FROM activity_event WHERE kind = 'QUEST_LOG_CREATED'") == 1);
}

TEST_CASE("the Codex shows the new rating straight away", "[integration][quests]") {
  Fixture f;
  const auto levelsBefore = f.scalar("SELECT COALESCE(SUM(count), 0) FROM agent_skill_stat WHERE agent_id = '" + GRYPHON +
                                     "' AND skill = 'OVERALL' AND quest_type = 'CODING'");
  REQUIRE(f.quests.createQuestLog("alice", Fixture::input()).has_value());
  CHECK(f.scalar("SELECT SUM(count) FROM agent_skill_stat WHERE agent_id = '" + GRYPHON +
                 "' AND skill = 'OVERALL' AND quest_type = 'CODING'") == levelsBefore + 1);
}

TEST_CASE("a user without an Adventurer is sent to the Costumer and nothing is written", "[integration][quests]") {
  Fixture f;
  const auto e = errorOf(f.quests.createQuestLog("carol", Fixture::input()));
  CHECK(e.error.message == "You need an Adventurer to go questing.");
  CHECK(e.error.code == GameErrorCode::Forbidden);
  CHECK(f.scalar("SELECT COUNT(*) FROM quest_log") == 0);
  CHECK(f.scalar("SELECT COUNT(*) FROM xp_entry WHERE user_id NOT LIKE 'seed:%'") == 0);
  CHECK(f.scalar("SELECT COUNT(*) FROM activity_event") == 0);
  CHECK(f.storedAggregates().empty());
  CHECK(f.ratingEvents.empty());
}

TEST_CASE("only approved Agents and real players can be written about", "[integration][quests]") {
  Fixture f;
  CHECK(errorOf(f.quests.createQuestLog("alice", Fixture::input("seed:agent:nobody"))).error.code == GameErrorCode::NotFound);
  f.db.exec("UPDATE agent SET status = 'PENDING' WHERE id = '" + CAIRN + "'");
  CHECK(errorOf(f.quests.createQuestLog("alice", Fixture::input(CAIRN))).error.code == GameErrorCode::NotFound);
  CHECK(errorOf(f.quests.createQuestLog("ghost", Fixture::input())).error.code == GameErrorCode::Forbidden);
  CHECK(errorOf(f.quests.createQuestLog("seed:npc:mossbeard", Fixture::input())).error.code == GameErrorCode::Forbidden);
  CHECK(f.scalar("SELECT COUNT(*) FROM quest_log") == 0);
}

TEST_CASE("a bad form reports every problem and writes nothing", "[integration][quests]") {
  Fixture f;
  auto in = Fixture::input();
  in.title = "no";
  in.overall = 9;
  in.body = "short";
  const auto e = errorOf(f.quests.createQuestLog("alice", in));
  CHECK(e.error.code == GameErrorCode::InvalidInput);
  CHECK(e.fields.size() == 3);
  CHECK(f.scalar("SELECT COUNT(*) FROM quest_log") == 0);
  CHECK(f.storedAggregates().empty());
}

TEST_CASE("one log per Agent and version: a repeat points at the existing one", "[integration][quests]") {
  Fixture f;
  const auto first = f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v1"));
  REQUIRE(first.has_value());
  const auto e = errorOf(f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v1")));
  CHECK(e.error.message == "You've already logged this quest. Update it instead?");
  CHECK(e.existingId == std::optional<std::string>{first->questLogId});
  CHECK(*f.quests.findOwn("alice", GRYPHON, "v1") == std::optional<std::string>{first->questLogId});
  CHECK_FALSE(f.quests.findOwn("alice", GRYPHON, "v2")->has_value());

  CHECK(f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v2")).has_value());   // another version is fine
  CHECK(f.quests.createQuestLog("alice", Fixture::input(CAIRN, "v1")).has_value());     // so is another Agent
  CHECK(f.quests.createQuestLog("bob", Fixture::input(GRYPHON, "v1")).has_value());     // and another author
  CHECK(f.totalXp("alice") == 750);   // the refused repeat paid nothing
  f.checkInvariants();
}

TEST_CASE("five new Quest Logs per rolling hour", "[integration][quests]") {
  Fixture f;
  for (int i = 0; i < QUEST_LOGS_PER_HOUR; ++i) {
    REQUIRE(f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v" + std::to_string(i))).has_value());
    f.clock.advance(std::chrono::minutes{1});
  }
  const auto sixth = errorOf(f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v9")));
  CHECK(sixth.error.code == GameErrorCode::RateLimited);
  CHECK(sixth.error.message == "You're doing that too often. Rest a while, adventurer.");
  CHECK(f.quests.createQuestLog("bob", Fixture::input(GRYPHON, "v9")).has_value());   // per user
  f.clock.advance(std::chrono::minutes{55});   // now an hour after the first log, which falls out of the window
  CHECK(f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v9")).has_value());
  CHECK_FALSE(f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v10")).has_value());
  f.checkInvariants();
}

TEST_CASE("editing moves the ratings by the difference, keeps the snapshot, and pays nothing", "[integration][quests]") {
  Fixture f;
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  const auto original = **f.quests.get(made->questLogId);
  f.clearEvents();

  auto edited = Fixture::input();
  edited.overall = 2;
  edited.skills = {{Skill::Coding, 5}, {Skill::Value, 4}};   // Speed dropped, Value added
  edited.questType = QuestType::Debugging;                    // and the Quest Type changed
  edited.title = "Second thoughts";
  REQUIRE(f.quests.updateQuestLog("alice", made->questLogId, edited).has_value());

  const auto now = **f.quests.get(made->questLogId);
  CHECK(now.input == edited);
  CHECK(now.snapshot == original.snapshot);
  CHECK(f.totalXp("alice") == 250);
  const auto agg = f.storedAggregates();
  CHECK(agg.size() == 3);
  CHECK(agg.at({GRYPHON, "OVERALL", "DEBUGGING"}) == std::pair<std::int64_t, std::int64_t>{2, 1});
  CHECK(agg.at({GRYPHON, "CODING", "DEBUGGING"}) == std::pair<std::int64_t, std::int64_t>{5, 1});
  CHECK(agg.at({GRYPHON, "VALUE", "DEBUGGING"}) == std::pair<std::int64_t, std::int64_t>{4, 1});
  f.checkInvariants();
  CHECK(f.ratingEvents == std::vector<std::string>{GRYPHON});
  CHECK(f.xpEvents.empty());
  CHECK(f.feed.empty());

  // Saving the same thing again changes nothing and announces nothing.
  f.clearEvents();
  REQUIRE(f.quests.updateQuestLog("alice", made->questLogId, edited).has_value());
  CHECK(f.ratingEvents.empty());
}

TEST_CASE("only the author edits, and the Agent and version can't move", "[integration][quests]") {
  Fixture f;
  const auto made = f.quests.createQuestLog("alice", Fixture::input(GRYPHON, "v1"));
  REQUIRE(made.has_value());
  CHECK(f.quests.updateQuestLog("bob", made->questLogId, Fixture::input(GRYPHON, "v1")).error().error.code == GameErrorCode::Forbidden);
  CHECK(f.quests.updateQuestLog("alice", "missing", Fixture::input(GRYPHON, "v1")).error().error.code == GameErrorCode::NotFound);
  CHECK(f.quests.updateQuestLog("alice", made->questLogId, Fixture::input(CAIRN, "v1")).error().error.code == GameErrorCode::InvalidInput);
  CHECK(f.quests.updateQuestLog("alice", made->questLogId, Fixture::input(GRYPHON, "v2")).error().error.code == GameErrorCode::InvalidInput);
  auto bad = Fixture::input(GRYPHON, "v1");
  bad.title = "x";
  CHECK_FALSE(f.quests.updateQuestLog("alice", made->questLogId, bad).error().fields.empty());
  CHECK((**f.quests.get(made->questLogId)).input == Fixture::input(GRYPHON, "v1"));
  f.checkInvariants();
}

TEST_CASE("deleting takes the ratings and the XP back out, and keeps level unlocks", "[integration][quests]") {
  Fixture f;
  f.setXp("alice", domain::guestSeedXp());   // level 13: one Quest Log from level 14
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  REQUIRE(made->levelAfter == 14);
  const auto unlockedBefore = f.scalar("SELECT COUNT(*) FROM item_unlock WHERE user_id = 'alice'");
  f.clearEvents();

  REQUIRE(f.quests.deleteQuestLog("alice", made->questLogId).has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM quest_log") == 0);
  CHECK(f.storedAggregates().empty());
  CHECK(f.totalXp("alice") == domain::guestSeedXp());
  CHECK(f.scalar("SELECT COUNT(*) FROM xp_entry WHERE reason = 'QUEST_LOG'") == 0);
  CHECK(f.scalar("SELECT COUNT(*) FROM item_unlock WHERE user_id = 'alice'") == unlockedBefore);   // permanent
  f.checkInvariants();
  CHECK(f.ratingEvents == std::vector<std::string>{GRYPHON});
  REQUIRE(f.xpEvents.size() == 1);
  CHECK(f.xpEvents[0].totalXp == domain::guestSeedXp());

  CHECK(f.quests.deleteQuestLog("alice", made->questLogId).error().code == GameErrorCode::NotFound);
}

TEST_CASE("only the author deletes", "[integration][quests]") {
  Fixture f;
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  CHECK(f.quests.deleteQuestLog("bob", made->questLogId).error().code == GameErrorCode::Forbidden);
  CHECK(f.quests.deleteQuestLog("kay", made->questLogId).error().code == GameErrorCode::Forbidden);   // even a Keeper: they hide instead
  CHECK(f.scalar("SELECT COUNT(*) FROM quest_log") == 1);
}

TEST_CASE("a Helpful vote pays the author 25 XP, can't be on your own log, and can be withdrawn", "[integration][quests]") {
  Fixture f;
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  CHECK(f.quests.toggleHelpful("alice", made->questLogId).error().code == GameErrorCode::Forbidden);
  CHECK(f.quests.toggleHelpful("bob", "missing").error().code == GameErrorCode::NotFound);
  CHECK(f.quests.toggleHelpful("ghost", made->questLogId).error().code == GameErrorCode::Forbidden);
  CHECK(f.totalXp("alice") == 250);

  REQUIRE(f.quests.toggleHelpful("bob", made->questLogId).value() == true);
  CHECK((**f.quests.get(made->questLogId)).helpfulCount == 1);
  CHECK(f.totalXp("alice") == 275);
  f.checkInvariants();

  REQUIRE(f.quests.toggleHelpful("bob", made->questLogId).value() == false);   // withdraw
  CHECK((**f.quests.get(made->questLogId)).helpfulCount == 0);
  CHECK(f.totalXp("alice") == 250);
  CHECK(f.scalar("SELECT COUNT(*) FROM helpful_vote") == 0);
  f.checkInvariants();

  REQUIRE(f.quests.toggleHelpful("bob", made->questLogId).value() == true);    // and vote again: paid again, once
  CHECK(f.totalXp("alice") == 275);
  f.checkInvariants();
}

TEST_CASE("Helpful XP stops at 250 a UTC day and starts again the next day", "[integration][quests]") {
  Fixture f;
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  for (int i = 0; i < 12; ++i) f.player("v" + std::to_string(i), "Voter" + std::to_string(i), false);
  for (int i = 0; i < 10; ++i) REQUIRE(f.quests.toggleHelpful("v" + std::to_string(i), made->questLogId).has_value());
  CHECK(f.totalXp("alice") == 250 + 250);

  REQUIRE(f.quests.toggleHelpful("v10", made->questLogId).value() == true);   // the vote counts, the XP doesn't
  CHECK((**f.quests.get(made->questLogId)).helpfulCount == 11);
  CHECK(f.totalXp("alice") == 500);
  REQUIRE(f.quests.toggleHelpful("v10", made->questLogId).value() == false);  // withdrawing an unpaid vote takes nothing
  CHECK(f.totalXp("alice") == 500);
  f.checkInvariants();

  f.clock.advance(std::chrono::hours{24});
  REQUIRE(f.quests.toggleHelpful("v11", made->questLogId).value() == true);   // a new UTC day
  CHECK(f.totalXp("alice") == 525);
  f.checkInvariants();
}

TEST_CASE("deleting a log revokes the helpful XP it earned too", "[integration][quests]") {
  Fixture f;
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  REQUIRE(f.quests.toggleHelpful("bob", made->questLogId).has_value());
  REQUIRE(f.quests.toggleHelpful("kay", made->questLogId).has_value());
  CHECK(f.totalXp("alice") == 300);
  REQUIRE(f.quests.deleteQuestLog("alice", made->questLogId).has_value());
  CHECK(f.totalXp("alice") == 0);
  CHECK(f.scalar("SELECT COUNT(*) FROM xp_entry WHERE user_id NOT LIKE 'seed:%'") == 0);
  CHECK(f.scalar("SELECT COUNT(*) FROM helpful_vote") == 0);   // cascaded
  f.checkInvariants();
}

TEST_CASE("the first Quest Log of a new Guest levels them up and unlocks gear", "[integration][quests]") {
  Fixture f;
  f.setXp("alice", domain::guestSeedXp());
  const auto out = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(out.has_value());
  CHECK(out->levelBefore == 13);
  CHECK(out->levelAfter == 14);
  CHECK(out->xpGained == 250);
  REQUIRE_FALSE(out->unlocked.empty());
  for (const auto& id : out->unlocked) {
    CHECK(f.scalar("SELECT COUNT(*) FROM item_unlock WHERE user_id = 'alice' AND item_id = '" + id + "' AND source = 'LEVEL'") == 1);
  }
  CHECK(f.unlockEvents == out->unlocked);
  REQUIRE(f.xpEvents.size() == 1);
  CHECK(f.xpEvents[0].oldLevel == 13);
  CHECK(f.xpEvents[0].newLevel == 14);

  // A second log does not unlock the same things again.
  f.clearEvents();
  const auto again = f.quests.createQuestLog("alice", Fixture::input(CAIRN));
  REQUIRE(again.has_value());
  CHECK(again->unlocked.empty());
  CHECK(f.unlockEvents.empty());
}

TEST_CASE("reaching a multiple of ten announces it in the feed", "[integration][quests]") {
  Fixture f;
  f.setXp("alice", domain::xpTable()[10] - 100);   // level 9, a Quest Log from level 10
  const auto out = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(out.has_value());
  CHECK(out->levelAfter == 10);
  REQUIRE(f.feed.size() == 2);
  CHECK(f.feed[0] == "Alice completed a Coding quest with Gryphon (4/5).");   // ordering inside the transaction: the quest first
  CHECK(f.feed[1] == "Alice reached Questing level 10!");
  CHECK(f.scalar("SELECT COUNT(*) FROM activity_event WHERE kind = 'LEVEL_UP'") == 1);

  f.clearEvents();
  f.setXp("bob", domain::xpTable()[11] - 100);   // 11 -> 12: no milestone
  REQUIRE(f.quests.createQuestLog("bob", Fixture::input()).has_value());
  CHECK(f.feed.size() == 1);
}

TEST_CASE("XP entries are unique by user, reason and reference, so a retry never pays twice", "[integration][quests]") {
  Fixture f;
  db::Transaction tx{f.db};
  const auto first = f.xp.award("alice", domain::XpReason::Note, "n1", 50);
  const auto second = f.xp.award("alice", domain::XpReason::Note, "n1", 50);
  const auto other = f.xp.award("alice", domain::XpReason::Note, "n2", 50);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  REQUIRE(other.has_value());
  CHECK(first->awarded);
  CHECK_FALSE(second->awarded);
  CHECK(second->amount == 0);
  CHECK(second->totalXp == 50);
  CHECK(other->awarded);
  tx.commit();
  CHECK(f.totalXp("alice") == 100);
  f.checkInvariants();
}

TEST_CASE("the XP ledger refuses to run outside a transaction", "[integration][quests]") {
  Fixture f;
  const auto r = f.xp.award("alice", domain::XpReason::Note, "n1", 50);
  REQUIRE_FALSE(r.has_value());
  CHECK(f.totalXp("alice") == 0);
  CHECK_FALSE(f.xp.award("alice", domain::XpReason::Note, "n1", 0).has_value());
}

TEST_CASE("repairing totals fixes drift from the ledger", "[integration][quests]") {
  Fixture f;
  REQUIRE(f.quests.createQuestLog("alice", Fixture::input()).has_value());
  f.db.exec("UPDATE user SET total_xp = 9999 WHERE id = 'alice'");
  f.db.exec("UPDATE user SET total_xp = 5 WHERE id = 'bob'");
  const auto fixed = f.xp.repairTotals();
  REQUIRE(fixed.has_value());
  CHECK(*fixed == 2);
  CHECK(f.totalXp("alice") == 250);
  CHECK(f.totalXp("bob") == 0);
  CHECK(*f.xp.repairTotals() == 0);
}

TEST_CASE("only a Keeper moderates, hiding takes a log off the board and excluding takes its ratings out", "[integration][quests]") {
  Fixture f;
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  const auto withLog = f.storedAggregates();
  CHECK(f.quests.setQuestLogModeration("alice", made->questLogId, true, true).error().code == GameErrorCode::Forbidden);
  CHECK(f.quests.setQuestLogModeration("alice", made->questLogId, true, true).error().message == "Only a Codex Keeper can do that.");
  CHECK(f.quests.setQuestLogModeration("kay", "missing", true, false).error().code == GameErrorCode::NotFound);
  CHECK(f.storedAggregates() == withLog);

  f.clearEvents();
  REQUIRE(f.quests.setQuestLogModeration("kay", made->questLogId, true, false).has_value());   // hidden stays counted (2.3)
  CHECK(f.storedAggregates() == withLog);
  CHECK(f.ratingEvents.empty());
  CHECK((**f.quests.get(made->questLogId)).hidden);
  CHECK(f.quests.toggleHelpful("bob", made->questLogId).error().code == GameErrorCode::NotFound);   // hidden: no votes

  REQUIRE(f.quests.setQuestLogModeration("kay", made->questLogId, true, true).has_value());    // excluded leaves the aggregates
  CHECK(f.storedAggregates().empty());
  CHECK(f.ratingEvents == std::vector<std::string>{GRYPHON});
  f.checkInvariants();

  // Editing an excluded log doesn't sneak its ratings back in.
  auto edit = Fixture::input();
  edit.overall = 1;
  REQUIRE(f.quests.updateQuestLog("alice", made->questLogId, edit).has_value());
  CHECK(f.storedAggregates().empty());
  f.checkInvariants();

  REQUIRE(f.quests.setQuestLogModeration("kay", made->questLogId, false, false).has_value());  // restored, with the edited rating
  CHECK(f.storedAggregates().at({GRYPHON, "OVERALL", "CODING"}) == std::pair<std::int64_t, std::int64_t>{1, 1});
  f.checkInvariants();

  // Deleting an excluded log must not take its ratings out a second time.
  REQUIRE(f.quests.setQuestLogModeration("kay", made->questLogId, false, true).has_value());
  REQUIRE(f.quests.deleteQuestLog("alice", made->questLogId).has_value());
  CHECK(f.storedAggregates().empty());
  f.checkInvariants();
}

TEST_CASE("a failure part-way leaves nothing behind", "[integration][quests]") {
  Fixture f;
  // A second log with a duplicate (user, agent, version) trips the UNIQUE index if the pre-check were skipped; here we
  // force a failure after the INSERT instead: make the XP ledger reject the award.
  f.db.exec("CREATE TRIGGER no_xp BEFORE INSERT ON xp_entry BEGIN SELECT RAISE(ABORT, 'ledger closed'); END");
  const auto r = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().error.code == GameErrorCode::StorageFailed);
  CHECK(r.error().error.message == "Something went wrong. Your quest scroll was not saved.");
  CHECK(f.scalar("SELECT COUNT(*) FROM quest_log") == 0);
  CHECK(f.storedAggregates().empty());
  CHECK(f.scalar("SELECT COUNT(*) FROM activity_event") == 0);
  CHECK(f.totalXp("alice") == 0);
  CHECK(f.ratingEvents.empty());
  CHECK(f.feed.empty());
  CHECK_FALSE(f.db.inTransaction());
}

// Task 3.6: random operations against a real database, then the stored aggregates equal a SQL recompute and
// total_xp equals the ledger sum, after every single operation.
TEST_CASE("50 random operations keep aggregates and XP consistent", "[integration][quests][random]") {
  for (unsigned seed = 1; seed <= 12; ++seed) {
    INFO("seed " << seed);
    Fixture f;
    std::mt19937 gen(seed);
    const std::vector<std::string> users{"alice", "bob", "kay"};
    const std::vector<std::string> agents{GRYPHON, CAIRN, "seed:agent:quillback"};
    std::vector<std::pair<std::string, std::string>> logs;   // (author, id)
    for (int step = 0; step < 50; ++step) {
      INFO("step " << step);
      const auto pick = [&](std::size_t n) { return static_cast<std::size_t>(gen() % n); };
      f.clock.advance(std::chrono::minutes{static_cast<int>(pick(40))});
      switch (pick(6)) {
        case 0:
        case 1: {   // create
          auto in = Fixture::input(agents[pick(agents.size())], pick(2) ? "v1" : "");
          in.questType = domain::QUEST_TYPES[pick(3)];
          in.overall = 1 + static_cast<int>(pick(5));
          in.skills.clear();
          for (const auto skill : domain::SKILLS) {
            if (pick(3) == 0) in.skills[skill] = 1 + static_cast<int>(pick(5));
          }
          const auto& author = users[pick(users.size())];
          if (auto r = f.quests.createQuestLog(author, in)) logs.emplace_back(author, r->questLogId);
          break;
        }
        case 2: {   // edit
          if (logs.empty()) break;
          const auto& [author, id] = logs[pick(logs.size())];
          auto in = (**f.quests.get(id)).input;
          in.overall = 1 + static_cast<int>(pick(5));
          in.questType = domain::QUEST_TYPES[pick(3)];
          in.skills.clear();
          for (const auto skill : domain::SKILLS) {
            if (pick(3) == 0) in.skills[skill] = 1 + static_cast<int>(pick(5));
          }
          CHECK(f.quests.updateQuestLog(author, id, in).has_value());
          break;
        }
        case 3: {   // delete
          if (logs.empty()) break;
          const auto index = pick(logs.size());
          CHECK(f.quests.deleteQuestLog(logs[index].first, logs[index].second).has_value());
          logs.erase(logs.begin() + static_cast<std::ptrdiff_t>(index));
          break;
        }
        case 4: {   // helpful vote (own logs and capped days are refused or unpaid; both fine)
          if (logs.empty()) break;
          (void)f.quests.toggleHelpful(users[pick(users.size())], logs[pick(logs.size())].second);
          break;
        }
        default: {  // moderate
          if (logs.empty()) break;
          const auto& id = logs[pick(logs.size())].second;
          CHECK(f.quests.setQuestLogModeration("kay", id, pick(2) == 0, pick(2) == 0).has_value());
          break;
        }
      }
      f.checkInvariants();
    }
  }
}

// --- XP from the other features, Guests, and reports on Quest Logs ---------------------------------------------------


TEST_CASE("a new Note earns 50 XP and a refused one earns nothing", "[integration][quests][xp]") {
  Fixture f;
  NoteService notes(f.db, f.clock, f.rng, f.bus);
  NewNote n;
  n.agentId = GRYPHON;
  n.text = "Explains things clearly and patiently.";
  const auto id = notes.createNote("alice", n);
  REQUIRE(id.has_value());
  CHECK(f.totalXp("alice") == 50);
  CHECK(f.scalar("SELECT COUNT(*) FROM xp_entry WHERE reason = 'NOTE' AND ref_id = '" + *id + "'") == 1);
  REQUIRE(f.xpEvents.size() == 1);
  CHECK(f.xpEvents[0].totalXp == 50);

  n.text = "short";
  CHECK_FALSE(notes.createNote("alice", n).has_value());
  CHECK(f.totalXp("alice") == 50);
  f.checkInvariants();
}

TEST_CASE("approving a proposal pays its proposer 500 XP; rejecting pays nothing", "[integration][quests][xp]") {
  Fixture f;
  AgentService agents(f.db, f.clock, f.rng, f.bus);
  KeeperService keeper(f.db, f.clock, f.rng, f.bus);
  domain::AgentInput in;
  in.name = "Lumen Scribe";
  in.vendor = "Candlewick";
  in.examine = "A tiny helper that glows when it is unsure.";
  const auto approved = agents.proposeAgent("alice", in);
  in.name = "Dim Scribe";
  const auto rejected = agents.proposeAgent("alice", in);
  REQUIRE(approved.has_value());
  REQUIRE(rejected.has_value());

  REQUIRE(keeper.moderateAgent("kay", *rejected, false).has_value());
  CHECK(f.totalXp("alice") == 0);
  f.clearEvents();
  REQUIRE(keeper.moderateAgent("kay", *approved, true).has_value());
  CHECK(f.totalXp("alice") == 500);
  CHECK(f.totalXp("kay") == 0);   // the Keeper earns nothing for deciding
  CHECK(f.scalar("SELECT COUNT(*) FROM xp_entry WHERE reason = 'AGENT_APPROVED' AND ref_id = '" + *approved + "'") == 1);
  REQUIRE(f.xpEvents.size() == 1);
  CHECK(f.xpEvents[0].userId == "alice");
  CHECK(f.feed.size() == 1);   // "joined the Grand Codex", plus no level milestone at 500 XP (level 5)
  f.checkInvariants();

  CHECK_FALSE(keeper.moderateAgent("kay", *approved, true).has_value());   // already decided: no second payment
  CHECK(f.totalXp("alice") == 500);
}

TEST_CASE("a seed Agent has no proposer, so approving one pays nobody", "[integration][quests][xp]") {
  Fixture f;
  KeeperService keeper(f.db, f.clock, f.rng, f.bus);
  f.db.exec("UPDATE agent SET status = 'PENDING' WHERE id = '" + CAIRN + "'");
  REQUIRE(keeper.moderateAgent("kay", CAIRN, true).has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM xp_entry WHERE user_id NOT LIKE 'seed:%'") == 0);
}

TEST_CASE("a new Guest starts at Questing level 13 from a seed ledger row", "[integration][quests][xp]") {
  Fixture f;
  ProfileService profile(f.db, f.clock, f.rng);
  const auto guest = profile.createGuest();
  REQUIRE(guest.has_value());
  CHECK(f.totalXp(guest->id) == domain::guestSeedXp());
  CHECK(domain::levelForXp(f.totalXp(guest->id)) == 13);
  CHECK(f.scalar("SELECT COUNT(*) FROM xp_entry WHERE user_id = '" + guest->id + "' AND reason = 'SEED' AND ref_id = 'seed'") == 1);
  f.checkInvariants();

  CHECK(f.totalXp("alice") == 0);   // players start at level 1
}

TEST_CASE("a Guest's first Quest Log is the level-up the demo relies on", "[integration][quests][xp]") {
  Fixture f;
  ProfileService profile(f.db, f.clock, f.rng);
  const auto guest = profile.createGuest();
  REQUIRE(guest.has_value());
  REQUIRE(f.adventurers.createAdventurer(guest->id, "Guesty").has_value());
  const auto out = f.quests.createQuestLog(guest->id, Fixture::input());
  REQUIRE(out.has_value());
  CHECK(out->levelBefore == 13);
  CHECK(out->levelAfter == 14);
  f.checkInvariants();
}

TEST_CASE("a Quest Log can be reported once, then a Keeper sees it in the queue", "[integration][quests][reports]") {
  Fixture f;
  KeeperService keeper(f.db, f.clock, f.rng, f.bus);
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  CHECK(keeper.reportContent("bob", ReportTarget::QuestLog, "missing", "spam").error().code == GameErrorCode::NotFound);
  REQUIRE(keeper.reportContent("bob", ReportTarget::QuestLog, made->questLogId, "this is an advert").has_value());
  CHECK(keeper.reportContent("bob", ReportTarget::QuestLog, made->questLogId, "again").error().message == "You have already reported that.");

  const auto q = keeper.queue("kay");
  REQUIRE(q.has_value());
  REQUIRE(q->reports.size() == 1);
  CHECK(q->reports[0].target == ReportTarget::QuestLog);
  CHECK(q->reports[0].targetText == "A solid helper");   // the log's title
  CHECK(q->reports[0].agentName == "Gryphon");
  CHECK(q->reports[0].reporterName == "Bobby");
  CHECK_FALSE(q->reports[0].targetHidden);

  REQUIRE(f.quests.setQuestLogModeration("kay", made->questLogId, true, false).has_value());
  CHECK(keeper.queue("kay")->reports[0].targetHidden);
  CHECK(keeper.reportContent("kay", ReportTarget::QuestLog, made->questLogId, "late").error().code == GameErrorCode::NotFound);   // hidden: can't be reported
}

TEST_CASE("an idle Guest's content leaves the Codex through the normal delete paths", "[integration][quests][guests]") {
  Fixture f;
  ProfileService profile(f.db, f.clock, f.rng);
  NoteService notes(f.db, f.clock, f.rng, f.bus);
  profile.setBeforeGuestDelete([&](const std::string& id) {
    REQUIRE(f.quests.purgeUser(id).has_value());
    REQUIRE(notes.retractAllVotes(id).has_value());
  });

  const auto guest = profile.createGuest();
  REQUIRE(guest.has_value());
  REQUIRE(f.adventurers.createAdventurer(guest->id, "Guesty").has_value());

  // The Guest writes a log, votes on Alice's log and on a Note.
  REQUIRE(f.quests.createQuestLog(guest->id, Fixture::input(CAIRN)).has_value());
  const auto alices = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(alices.has_value());
  REQUIRE(f.quests.toggleHelpful(guest->id, alices->questLogId).value());
  NewNote n;
  n.agentId = GRYPHON;
  n.text = "A remark from somebody else entirely.";
  const auto noteId = notes.createNote("bob", n);
  REQUIRE(noteId.has_value());
  REQUIRE(notes.voteOnNote(guest->id, *noteId, domain::VoteValue::Disagree).has_value());
  CHECK(f.totalXp("alice") == 275);
  CHECK(f.scalar("SELECT disagree FROM note WHERE id = '" + *noteId + "'") == 1);

  f.clock.advance(std::chrono::hours{25});   // the Guest goes idle
  const auto removed = profile.cleanupGuests();
  REQUIRE(removed.has_value());
  CHECK(*removed == 1);
  CHECK(f.scalar("SELECT COUNT(*) FROM user WHERE id = '" + guest->id + "'") == 0);
  CHECK(f.scalar("SELECT COUNT(*) FROM quest_log WHERE author_id = '" + guest->id + "'") == 0);
  CHECK(f.scalar("SELECT helpful_count FROM quest_log WHERE id = '" + alices->questLogId + "'") == 0);
  CHECK(f.scalar("SELECT disagree FROM note WHERE id = '" + *noteId + "'") == 0);
  CHECK(f.totalXp("alice") == 250);   // the Guest's helpful vote is taken back
  f.checkInvariants();   // aggregates equal a recompute; no orphaned rating from the Guest's log
  CHECK(f.storedAggregates().count({CAIRN, "OVERALL", "CODING"}) == 0);
}

TEST_CASE("without the cleanup hook a deleted Guest's ratings would be orphaned (why the hook exists)", "[integration][quests][guests]") {
  Fixture f;
  ProfileService profile(f.db, f.clock, f.rng);   // no hook installed
  const auto guest = profile.createGuest();
  REQUIRE(guest.has_value());
  REQUIRE(f.adventurers.createAdventurer(guest->id, "Guesty").has_value());
  REQUIRE(f.quests.createQuestLog(guest->id, Fixture::input(CAIRN)).has_value());
  f.clock.advance(std::chrono::hours{25});
  REQUIRE(profile.cleanupGuests().has_value());
  CHECK_FALSE(f.storedAggregates() == f.recomputedAggregates());   // the stored ratings outlive the log
}

TEST_CASE("a user's XP is read back from the profile, and a Guest starts at level 13", "[integration][quests][xp]") {
  Fixture f;
  ProfileService profile(f.db, f.clock, f.rng);
  REQUIRE(f.quests.createQuestLog("alice", Fixture::input()).has_value());
  CHECK(profile.getUser("alice")->totalXp == 250);
  CHECK(profile.getUser("bob")->totalXp == 0);
  const auto guest = profile.createGuest();
  REQUIRE(guest.has_value());
  CHECK(profile.getUser(guest->id)->totalXp == domain::guestSeedXp());
  CHECK(domain::levelForXp(profile.getUser(guest->id)->totalXp) == 13);
}

// --- The Tavern board and the card's top logs ---------------------------------------------------------------------------

#include "domain/snapshot.hpp"

namespace {
// 16 logs from four authors, one every minute: the oldest is "log00".
struct Board {
  Fixture f;
  std::vector<std::string> ids;   // oldest first
  Board() {
    for (int u = 0; u < 4; ++u) f.player("w" + std::to_string(u), "Writer" + std::to_string(u), true);
    for (int i = 0; i < 16; ++i) {
      auto in = Fixture::input(i % 2 == 0 ? GRYPHON : CAIRN, "v" + std::to_string(i));
      in.questType = i % 4 == 0 ? QuestType::Writing : QuestType::Coding;
      in.title = "Quest log number " + std::string(i < 10 ? "0" : "") + std::to_string(i);
      const auto r = f.quests.createQuestLog("w" + std::to_string(i % 4), in);
      REQUIRE(r.has_value());
      ids.push_back(r->questLogId);
      f.clock.advance(std::chrono::minutes{1});
    }
  }
  std::vector<std::string> titles(const QuestPage& p) {
    std::vector<std::string> out;
    for (const auto& l : p.logs) out.push_back(l.input.title);
    return out;
  }
};
}  // namespace

TEST_CASE("the board lists newest first, twelve to a page", "[integration][quests][board]") {
  Board b;
  const auto p1 = b.f.quests.list({}, "alice");
  REQUIRE(p1.has_value());
  CHECK(p1->total == 16);
  CHECK(p1->logs.size() == 12);
  CHECK(p1->slice.pages == 2);
  CHECK(p1->logs.front().input.title == "Quest log number 15");
  CHECK(p1->logs.back().input.title == "Quest log number 04");
  QuestListQuery q;
  q.page = 1;
  const auto p2 = b.f.quests.list(q, "alice");
  REQUIRE(p2.has_value());
  CHECK(p2->logs.size() == 4);
  CHECK(p2->logs.front().input.title == "Quest log number 03");
  CHECK(p2->logs.back().input.title == "Quest log number 00");
  q.page = 99;   // past the end: the last page
  const auto clamped = b.f.quests.list(q, "alice");
  CHECK(clamped->slice.page == 1);
  CHECK(clamped->logs.size() == 4);
}

TEST_CASE("the board filters by Agent and Quest Type", "[integration][quests][board]") {
  Board b;
  QuestListQuery q;
  q.agentId = GRYPHON;
  const auto gryphon = b.f.quests.list(q, "");
  REQUIRE(gryphon.has_value());
  CHECK(gryphon->total == 8);
  for (const auto& l : gryphon->logs) CHECK(l.input.agentId == GRYPHON);
  q = {};
  q.questType = QuestType::Writing;
  CHECK(b.f.quests.list(q, "")->total == 4);   // i = 0, 4, 8, 12
  q.agentId = CAIRN;
  CHECK(b.f.quests.list(q, "")->total == 0);   // those are all Gryphon logs
  q.agentId = GRYPHON;
  CHECK(b.f.quests.list(q, "")->total == 4);
  q = {};
  q.questType = QuestType::Debugging;
  const auto none = b.f.quests.list(q, "");
  REQUIRE(none.has_value());
  CHECK(none->total == 0);
  CHECK(none->logs.empty());
  CHECK(none->slice.pages == 1);   // an empty board still has one (empty) page
}

TEST_CASE("most helpful comes first, ties newest first", "[integration][quests][board]") {
  Board b;
  b.f.player("fan1", "Fan1", false);
  b.f.player("fan2", "Fan2", false);
  REQUIRE(b.f.quests.toggleHelpful("fan1", b.ids[3]).has_value());
  REQUIRE(b.f.quests.toggleHelpful("fan2", b.ids[3]).has_value());
  REQUIRE(b.f.quests.toggleHelpful("fan1", b.ids[7]).has_value());
  REQUIRE(b.f.quests.toggleHelpful("fan1", b.ids[1]).has_value());
  QuestListQuery q;
  q.sort = QuestSort::MostHelpful;
  const auto p = b.f.quests.list(q, "fan1");
  REQUIRE(p.has_value());
  CHECK(p->logs[0].id == b.ids[3]);   // two votes
  CHECK(p->logs[1].id == b.ids[7]);   // one vote, newer
  CHECK(p->logs[2].id == b.ids[1]);   // one vote, older
  CHECK(p->logs[3].id == b.ids[15]);  // then nobody voted: newest first
  CHECK(p->helpedByViewer == std::set<std::string>{b.ids[3], b.ids[7], b.ids[1]});
  CHECK(b.f.quests.list(q, "fan2")->helpedByViewer == std::set<std::string>{b.ids[3]});
  CHECK(b.f.quests.list(q, "")->helpedByViewer.empty());   // no viewer
}

TEST_CASE("hidden logs and logs of Agents that left the Codex are not on the board", "[integration][quests][board]") {
  Board b;
  REQUIRE(b.f.quests.setQuestLogModeration("kay", b.ids[15], true, false).has_value());
  auto p = b.f.quests.list({}, "");
  CHECK(p->total == 15);
  CHECK(p->logs.front().input.title == "Quest log number 14");
  b.f.db.exec("UPDATE agent SET status = 'REJECTED' WHERE id = '" + CAIRN + "'");
  p = b.f.quests.list({}, "");
  CHECK(p->total == 8);   // only the 8 Gryphon logs remain (the hidden one, number 15, was Cairn's anyway)
  for (const auto& l : p->logs) CHECK(l.input.agentId == GRYPHON);
}

TEST_CASE("an excluded log is still on the board; only hiding removes it", "[integration][quests][board]") {
  Board b;
  REQUIRE(b.f.quests.setQuestLogModeration("kay", b.ids[15], false, true).has_value());
  CHECK(b.f.quests.list({}, "")->total == 16);
}

TEST_CASE("an Agent's card shows its five most helpful logs", "[integration][quests][board]") {
  Board b;
  for (int i = 0; i < 6; ++i) b.f.player("fan" + std::to_string(i), "Fan" + std::to_string(i), false);
  // Gryphon logs are the even ones. Give ids[0] three votes, ids[2] two, ids[4] one; ids[6..14] none.
  for (int v = 0; v < 3; ++v) REQUIRE(b.f.quests.toggleHelpful("fan" + std::to_string(v), b.ids[0]).has_value());
  for (int v = 0; v < 2; ++v) REQUIRE(b.f.quests.toggleHelpful("fan" + std::to_string(v), b.ids[2]).has_value());
  REQUIRE(b.f.quests.toggleHelpful("fan0", b.ids[4]).has_value());
  REQUIRE(b.f.quests.toggleHelpful("fan0", b.ids[5]).has_value());   // a Cairn log: must not appear for Gryphon

  const auto top = b.f.quests.topHelpful(GRYPHON);
  REQUIRE(top.has_value());
  REQUIRE(top->size() == 5);
  CHECK((*top)[0].id == b.ids[0]);
  CHECK((*top)[1].id == b.ids[2]);
  CHECK((*top)[2].id == b.ids[4]);
  CHECK((*top)[3].id == b.ids[14]);   // no votes left: newest first
  CHECK((*top)[4].id == b.ids[12]);
  for (const auto& l : *top) CHECK(l.input.agentId == GRYPHON);

  REQUIRE(b.f.quests.setQuestLogModeration("kay", b.ids[0], true, false).has_value());   // hidden: gone from the top
  CHECK((*b.f.quests.topHelpful(GRYPHON))[0].id == b.ids[2]);
  CHECK(b.f.quests.topHelpful(GRYPHON, 2)->size() == 2);
  CHECK(b.f.quests.topHelpful("seed:agent:mossling")->empty());
}

TEST_CASE("whether you marked a log Helpful can be asked directly", "[integration][quests][board]") {
  Board b;
  b.f.player("fan", "Fan", false);
  CHECK_FALSE(*b.f.quests.hasVoted("fan", b.ids[0]));
  REQUIRE(b.f.quests.toggleHelpful("fan", b.ids[0]).has_value());
  CHECK(*b.f.quests.hasVoted("fan", b.ids[0]));
  CHECK_FALSE(*b.f.quests.hasVoted("w1", b.ids[0]));
}

TEST_CASE("a saved snapshot draws the author's Adventurer as they were", "[integration][quests][board]") {
  Fixture f;
  auto adventurer = *f.adventurers.getActive("alice");
  REQUIRE(adventurer.has_value());
  const auto made = f.quests.createQuestLog("alice", Fixture::input());
  REQUIRE(made.has_value());
  const auto snap = domain::snapshotFromJson((**f.quests.get(made->questLogId)).snapshot);
  REQUIRE(snap.has_value());
  CHECK(snap->name == "Alice");
  CHECK(snap->appearance == adventurer->appearance);
  CHECK(snap->loadout == adventurer->loadout);
}
