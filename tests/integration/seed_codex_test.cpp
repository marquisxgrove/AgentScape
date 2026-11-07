#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "domain/stat_table.hpp"
#include "services/codex_store.hpp"

using namespace as;
using namespace as::db;
using namespace as::services;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};

Database freshSeededDb() {
  auto d = Database::open(":memory:");
  REQUIRE(d.has_value());
  REQUIRE(applyMigrations(*d).has_value());
  REQUIRE(runSeed(*d, NOW).has_value());
  return std::move(*d);
}

std::int64_t scalar(Database& db, std::string_view sql) {
  auto st = db.prepare(sql);
  REQUIRE(st.step());
  return st.columnInt(0);
}

struct Counts {
  std::int64_t agents, stats, statSum, statCount, notes, users;
  bool operator==(const Counts&) const = default;
};
Counts countsOf(Database& db) {
  return {scalar(db, "SELECT COUNT(*) FROM agent"),
          scalar(db, "SELECT COUNT(*) FROM agent_skill_stat"),
          scalar(db, "SELECT COALESCE(SUM(sum),0) FROM agent_skill_stat"),
          scalar(db, "SELECT COALESCE(SUM(count),0) FROM agent_skill_stat"),
          scalar(db, "SELECT COUNT(*) FROM note"),
          scalar(db, "SELECT COUNT(*) FROM user")};
}
}  // namespace

TEST_CASE("the seed fills the Codex: at least 12 approved agents, most skills ranked", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  const auto agents = listAgents(db, AgentStatus::Approved);
  REQUIRE(agents.has_value());
  CHECK(agents->size() >= 12);
  int models = 0;
  int tools = 0;
  for (const auto& a : *agents) {
    (a.kind == AgentKind::Model ? models : tools)++;
    CHECK(a.id == "seed:agent:" + a.slug);
    CHECK(a.examine.size() <= 200);
    REQUIRE(a.website.has_value());
    CHECK(a.website->rfind("https://", 0) == 0);

    const auto table = loadStats(db, a.id);
    REQUIRE(table.has_value());
    const auto levels = domain::levelsFor(*table, std::nullopt);
    CHECK(levels.overall.has_value());
    int ranked = 0;
    for (const auto& l : levels.skills) ranked += l.has_value();
    CHECK(ranked >= 8);                       // "most Skills are ranked"
    CHECK(levels.total >= 10);
  }
  CHECK(models >= 6);
  CHECK(tools >= 6);
}

TEST_CASE("the seed has enough Banes for the Hall of Hallucinations", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  CHECK(scalar(db, "SELECT COUNT(*) FROM note WHERE kind = 'BANE' AND hidden = 0 AND agree + disagree >= 3") >= 13);
  CHECK(scalar(db, "SELECT COUNT(*) FROM note WHERE kind = 'BOON'") >= 14);
  // Every note is from an NPC and within the length limits.
  CHECK(scalar(db, "SELECT COUNT(*) FROM note n JOIN user u ON u.id = n.author_id WHERE u.kind != 'NPC'") == 0);
  CHECK(scalar(db, "SELECT COUNT(*) FROM note WHERE length(text) < 10 OR length(text) > 280") == 0);
}

TEST_CASE("seeding twice is a no-op", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  const Counts before = countsOf(db);
  REQUIRE(runSeed(db, NOW).has_value());
  CHECK(countsOf(db) == before);
}

TEST_CASE("a reseed keeps the ratings players added", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  const auto stat = [&] {
    return domain::scopedAgg(*loadStats(db, "seed:agent:quillback"), domain::StatKey{domain::Skill::Coding},
                             domain::QuestType::Coding);
  };
  const domain::Agg baseline = stat();
  REQUIRE(baseline.count >= 2);

  {  // a player's Quest Log adds a 5-star Coding rating
    Transaction tx{db};
    const RatingDelta d[] = {{domain::StatKey{domain::Skill::Coding}, 5, 1}};
    REQUIRE(applyRatingDelta(db, "seed:agent:quillback", domain::QuestType::Coding, d).has_value());
    tx.commit();
  }
  CHECK(stat() == domain::Agg{baseline.sum + 5, baseline.count + 1});

  REQUIRE(runSeed(db, NOW).has_value());   // --reseed
  CHECK(stat() == domain::Agg{baseline.sum + 5, baseline.count + 1});   // the player's rating survives
  REQUIRE(runSeed(db, NOW).has_value());
  CHECK(stat() == domain::Agg{baseline.sum + 5, baseline.count + 1});   // ...however often it runs
}

TEST_CASE("a reseed restores a seeded baseline that was changed, without touching player ratings", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  // Simulate an older snapshot: the stored baseline differs from the file, plus a player rating on top.
  db.exec("UPDATE agent_skill_stat SET sum = sum + 1, count = count + 1, seed_sum = seed_sum + 1, seed_count = seed_count + 1 "
          "WHERE agent_id = 'seed:agent:spellforge' AND skill = 'SPEED' AND quest_type = 'OTHER'");
  {
    Transaction tx{db};
    const RatingDelta d[] = {{domain::StatKey{domain::Skill::Speed}, 4, 1}};
    REQUIRE(applyRatingDelta(db, "seed:agent:spellforge", domain::QuestType::Other, d).has_value());
    tx.commit();
  }
  auto row = [&] {
    auto st = db.prepare("SELECT sum, count, seed_sum, seed_count FROM agent_skill_stat "
                         "WHERE agent_id = 'seed:agent:spellforge' AND skill = 'SPEED' AND quest_type = 'OTHER'");
    REQUIRE(st.step());
    return std::array<std::int64_t, 4>{st.columnInt(0), st.columnInt(1), st.columnInt(2), st.columnInt(3)};
  };
  const auto stale = row();
  REQUIRE(runSeed(db, NOW).has_value());
  const auto fresh = row();
  CHECK(fresh[2] == stale[2] - 1);            // the baseline went back to the file's value
  CHECK(fresh[3] == stale[3] - 1);
  CHECK(fresh[0] - fresh[2] == 4);            // exactly the player's 4 stars remain on top
  CHECK(fresh[1] - fresh[3] == 1);
}

TEST_CASE("a reseed never touches the player's own Agents and Notes", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('p1', 'Marquis', 'PLAYER', 't', 't')");
  db.exec("INSERT INTO agent(id, slug, name, vendor, kind, examine, status, proposed_by_id, created_at, updated_at) "
          "VALUES ('01PLAYERAGENT', 'my-tool', 'My Tool', 'Me', 'TOOL', 'Mine.', 'PENDING', 'p1', 't', 't')");
  db.exec("INSERT INTO note(id, agent_id, author_id, kind, text, created_at) "
          "VALUES ('01PLAYERNOTE', 'seed:agent:spellforge', 'p1', 'BOON', 'A note the player wrote themselves.', 't')");
  const auto before = scalar(db, "SELECT COUNT(*) FROM agent WHERE proposed_by_id IS NOT NULL");
  REQUIRE(runSeed(db, NOW).has_value());
  CHECK(scalar(db, "SELECT COUNT(*) FROM agent WHERE proposed_by_id IS NOT NULL") == before);
  CHECK(scalar(db, "SELECT COUNT(*) FROM agent WHERE id = '01PLAYERAGENT' AND status = 'PENDING' AND name = 'My Tool'") == 1);
  CHECK(scalar(db, "SELECT COUNT(*) FROM note WHERE id = '01PLAYERNOTE' AND text = 'A note the player wrote themselves.'") == 1);
}

TEST_CASE("a reseed keeps votes, hidden notes and Keeper decisions", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('p1', 'Marquis', 'PLAYER', 't', 't')");
  db.exec("UPDATE note SET hidden = 1, agree = agree + 1 WHERE id = 'seed:note:spellforge-bane-1'");
  db.exec("INSERT INTO note_vote(note_id, user_id, value) VALUES ('seed:note:spellforge-boon-1', 'p1', 1)");
  db.exec("UPDATE note SET agree = agree + 1 WHERE id = 'seed:note:spellforge-boon-1'");
  db.exec("UPDATE agent SET status = 'REJECTED' WHERE id = 'seed:agent:grimble'");
  const auto agree = scalar(db, "SELECT agree FROM note WHERE id = 'seed:note:spellforge-boon-1'");

  REQUIRE(runSeed(db, NOW).has_value());
  CHECK(scalar(db, "SELECT hidden FROM note WHERE id = 'seed:note:spellforge-bane-1'") == 1);
  CHECK(scalar(db, "SELECT agree FROM note WHERE id = 'seed:note:spellforge-boon-1'") == agree);
  CHECK(scalar(db, "SELECT COUNT(*) FROM note_vote WHERE user_id = 'p1'") == 1);
  CHECK(scalar(db, "SELECT COUNT(*) FROM agent WHERE id = 'seed:agent:grimble' AND status = 'REJECTED'") == 1);
}

TEST_CASE("a seed Agent whose slug a player already took is skipped, not fatal", "[integration][seed][codex]") {
  auto d = Database::open(":memory:");
  REQUIRE(d.has_value());
  REQUIRE(applyMigrations(*d).has_value());
  d->exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('p1', 'Marquis', 'PLAYER', 't', 't')");
  d->exec("INSERT INTO agent(id, slug, name, vendor, kind, examine, status, proposed_by_id, created_at, updated_at) "
          "VALUES ('01PLAYERKIRO', 'spellforge', 'Spellforge', 'Me', 'TOOL', 'The player got here first.', 'APPROVED', 'p1', 't', 't')");
  const auto report = runSeed(*d, NOW);
  REQUIRE(report.has_value());
  CHECK(report->agentsSkipped == 1);
  CHECK(scalar(*d, "SELECT COUNT(*) FROM agent WHERE slug = 'spellforge'") == 1);
  CHECK(scalar(*d, "SELECT COUNT(*) FROM agent WHERE id = '01PLAYERKIRO' AND vendor = 'Me'") == 1);   // untouched
  CHECK(scalar(*d, "SELECT COUNT(*) FROM agent_skill_stat WHERE agent_id = 'seed:agent:spellforge'") == 0);  // no orphan rows
  CHECK(scalar(*d, "SELECT COUNT(*) FROM note WHERE agent_id = 'seed:agent:spellforge'") == 0);
  CHECK(scalar(*d, "SELECT COUNT(*) FROM agent") == 14);
}

TEST_CASE("bad Codex seed files are rejected whole, before anything is written", "[integration][seed][codex]") {
  const std::string npcs = R"({"snapshotAt":"2026-10-01T00:00:00Z","npcs":[{"id":"seed:npc:a","name":"Ann"}]})";
  const std::string agents =
      R"({"snapshotAt":"2026-10-01T00:00:00Z","agents":[{"slug":"x","name":"X","vendor":"V","kind":"TOOL","examine":"Fine.","website":"https://x.example.com"}]})";
  const auto rejects = [&](std::vector<SeedFile> files) {
    auto d = Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(applyMigrations(*d).has_value());
    const auto r = runSeed(*d, files, NOW);
    CHECK_FALSE(r.has_value());
    if (!r) CHECK_FALSE(r.error().detail.empty());
    CHECK(scalar(*d, "SELECT COUNT(*) FROM agent") == 0);
    CHECK(scalar(*d, "SELECT COUNT(*) FROM user") == 0);
  };
  const auto agentWith = [&](const std::string& field, const std::string& value) {
    std::string doc = agents;
    const auto at = doc.find(field);
    REQUIRE(at != std::string::npos);
    doc.replace(at, doc.find(',', at) - at, value);
    return doc;
  };
  rejects({{"npcs.json", npcs}, {"agents.json", agentWith("\"slug\":\"x\"", "\"slug\":\"not-x\"")}});            // slug != slugify(name)
  rejects({{"npcs.json", npcs}, {"agents.json", agentWith("\"kind\":\"TOOL\"", "\"kind\":\"ROBOT\"")}});
  rejects({{"npcs.json", npcs}, {"agents.json", agentWith("\"website\":\"https://x.example.com\"", "\"website\":\"http://x.example.com\"")}});  // http
  rejects({{"npcs.json", npcs}, {"agents.json", agents},
           {"ratings.json", R"({"snapshotAt":"2026-10-01T00:00:00Z","agents":{"x":{"CODING":{"CODING":[7,1]}}}})"}});   // 7 > 5*1
  rejects({{"npcs.json", npcs}, {"agents.json", agents},
           {"ratings.json", R"({"snapshotAt":"2026-10-01T00:00:00Z","agents":{"ghost":{"CODING":{"CODING":[3,1]}}}})"}});   // unknown agent
  rejects({{"npcs.json", npcs}, {"agents.json", agents},
           {"ratings.json", R"({"snapshotAt":"2026-10-01T00:00:00Z","agents":{"x":{"NONSENSE":{"CODING":[3,1]}}}})"}});
  rejects({{"npcs.json", npcs}, {"agents.json", agents},
           {"notes.json", R"({"snapshotAt":"2026-10-01T00:00:00Z","notes":[{"id":"seed:note:n1","agent":"x","author":"seed:npc:nobody","kind":"BOON","text":"A fine long note here.","agree":1,"disagree":0}]})"}});   // unknown author
  rejects({{"npcs.json", npcs}, {"agents.json", agents},
           {"notes.json", R"({"snapshotAt":"2026-10-01T00:00:00Z","notes":[{"id":"seed:note:n1","agent":"x","author":"seed:npc:a","kind":"BOON","text":"short","agree":1,"disagree":0}]})"}});   // too short
  rejects({{"agents.json", agents}, {"notes.json", R"({"snapshotAt":"2026-10-01T00:00:00Z","notes":[{"id":"seed:note:n1","agent":"x","author":"seed:npc:a","kind":"BOON","text":"A fine long note here.","agree":1,"disagree":0}]})"}});   // npcs missing
}

TEST_CASE("a reseed retires seed Agents that left the seed, without deleting them", "[integration][seed][codex]") {
  auto db = freshSeededDb();
  db.exec("INSERT INTO agent(id, slug, name, vendor, kind, examine, status, created_at, updated_at) "
          "VALUES ('seed:agent:old-one', 'old-one', 'Old One', 'Gone', 'MODEL', 'From an older seed.', 'APPROVED', 't', 't')");
  db.exec("INSERT INTO agent(id, slug, name, vendor, kind, examine, status, created_at, updated_at) "
          "VALUES ('01PLAYERMADE', 'player-made', 'Player Made', 'Me', 'TOOL', 'Mine.', 'APPROVED', 't', 't')");
  REQUIRE(runSeed(db, NOW).has_value());
  CHECK(scalar(db, "SELECT COUNT(*) FROM agent WHERE id = 'seed:agent:old-one' AND status = 'REJECTED'") == 1);
  CHECK(scalar(db, "SELECT COUNT(*) FROM agent WHERE id = '01PLAYERMADE' AND status = 'APPROVED'") == 1);
  CHECK(scalar(db, "SELECT COUNT(*) FROM agent WHERE id = 'seed:agent:quillback' AND status = 'APPROVED'") == 1);
}
