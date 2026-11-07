#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "services/codex_cache.hpp"
#include "services/codex_service.hpp"
#include "services/note_service.hpp"

using namespace as;
using namespace as::services;
using domain::VoteValue;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};
const std::string AGENT = "seed:agent:gryphon";

struct Fixture {
  db::Database db;
  core::FixedClock clock{NOW};
  core::SplitMix64 rng{7};
  app::EventBus bus;
  CodexCache cache;
  CodexService codex;
  NoteService svc;
  std::vector<std::string> changed;
  app::Subscription sub;

  Fixture() : db(open()), cache(bus), codex(db, cache), svc(db, clock, rng, bus) {
    user("alice");
    user("bob");
    sub = app::Subscription{bus, bus.subscribe<app::NotesChanged>([this](const app::NotesChanged& e) { changed.push_back(e.agentId); })};
  }

  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, NOW).has_value());
    return std::move(*d);
  }
  void user(const std::string& id) {
    db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('" + id + "', '" + id + "', 'PLAYER', 't', 't')");
  }
  std::int64_t scalar(const std::string& sql) {
    auto st = db.prepare(sql);
    REQUIRE(st.step());
    return st.columnInt(0);
  }
  NewNote note(const std::string& text = "Writes tidy code with sensible names.") {
    NewNote n;
    n.agentId = AGENT;
    n.text = text;
    return n;
  }
  // Counts on a Note must always equal its vote rows.
  void checkCounts(const std::string& id) {
    CHECK(scalar("SELECT agree FROM note WHERE id = '" + id + "'") ==
          scalar("SELECT COUNT(*) FROM note_vote WHERE note_id = '" + id + "' AND value = 1"));
    CHECK(scalar("SELECT disagree FROM note WHERE id = '" + id + "'") ==
          scalar("SELECT COUNT(*) FROM note_vote WHERE note_id = '" + id + "' AND value = -1"));
  }
};
}  // namespace

TEST_CASE("a Note is attached to the Agent and shows on its card", "[integration][notes]") {
  Fixture f;
  NewNote n = f.note("Remembers my project rules between sessions.");
  n.kind = NoteKind::Bane;
  n.skill = domain::Skill::Context;
  n.version = "  v2  ";
  const auto id = f.svc.createNote("alice", n);
  REQUIRE(id.has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM note WHERE id = '" + *id + "' AND version = 'v2' AND skill = 'CONTEXT' AND kind = 'BANE' AND author_id = 'alice'") == 1);
  CHECK(f.changed == std::vector<std::string>{AGENT});
  const auto banes = f.codex.notes(AGENT, NoteKind::Bane);
  REQUIRE(banes.has_value());
  CHECK(std::any_of(banes->begin(), banes->end(), [&](const NoteView& v) { return v.id == *id; }));
}

TEST_CASE("bad Notes are refused and nothing is written", "[integration][notes]") {
  Fixture f;
  const auto before = f.scalar("SELECT COUNT(*) FROM note");
  CHECK_FALSE(f.svc.createNote("alice", f.note("too short")).has_value());
  CHECK_FALSE(f.svc.createNote("alice", f.note(std::string(281, 'a'))).has_value());
  CHECK_FALSE(f.svc.createNote("alice", f.note("two\nlines of text here")).has_value());
  NewNote noAgent = f.note();
  noAgent.agentId = "seed:agent:nobody";
  CHECK(f.svc.createNote("alice", noAgent).error().code == GameErrorCode::NotFound);
  f.db.exec("UPDATE agent SET status = 'PENDING' WHERE id = '" + AGENT + "'");
  CHECK(f.svc.createNote("alice", f.note()).error().code == GameErrorCode::NotFound);   // not approved
  f.db.exec("UPDATE agent SET status = 'APPROVED' WHERE id = '" + AGENT + "'");
  CHECK(f.svc.createNote("ghost", f.note()).error().code == GameErrorCode::Forbidden);  // no such user
  CHECK(f.scalar("SELECT COUNT(*) FROM note") == before);
  CHECK(f.changed.empty());
}

TEST_CASE("markup in a Note is stored as the plain text it is", "[integration][notes]") {
  Fixture f;
  const auto id = f.svc.createNote("alice", f.note("<col=red>not markup</col> <b>**x**</b> &amp;"));
  REQUIRE(id.has_value());
  auto st = f.db.prepare("SELECT text FROM note WHERE id = ?");
  st.bind(1, *id);
  REQUIRE(st.step());
  CHECK(st.columnText(0) == "<col=red>not markup</col> <b>**x**</b> &amp;");
}

TEST_CASE("ten Notes per rolling hour, then rest", "[integration][notes]") {
  Fixture f;
  for (int i = 0; i < NOTES_PER_HOUR; ++i) {
    REQUIRE(f.svc.createNote("alice", f.note("Note number " + std::to_string(i) + " is fine.")).has_value());
    f.clock.advance(std::chrono::seconds{60});
  }
  const auto eleventh = f.svc.createNote("alice", f.note());
  REQUIRE_FALSE(eleventh.has_value());
  CHECK(eleventh.error().code == GameErrorCode::RateLimited);
  CHECK(eleventh.error().message == "You're doing that too often. Rest a while, adventurer.");
  CHECK(f.svc.createNote("bob", f.note()).has_value());   // the limit is per user
  f.clock.advance(std::chrono::minutes{50});               // exactly an hour after the first Note, which falls out of the window
  CHECK(f.svc.createNote("alice", f.note()).has_value());
  CHECK_FALSE(f.svc.createNote("alice", f.note()).has_value());   // but only one slot opened
}

TEST_CASE("a refused Note does not use up a slot", "[integration][notes]") {
  Fixture f;
  for (int i = 0; i < 30; ++i) (void)f.svc.createNote("alice", f.note("short"));
  CHECK(f.svc.createNote("alice", f.note()).has_value());
}

TEST_CASE("one vote per user: set, change, retract, with counts that always match", "[integration][notes]") {
  Fixture f;
  const auto id = *f.svc.createNote("alice", f.note());
  f.changed.clear();

  REQUIRE(f.svc.voteOnNote("bob", id, VoteValue::Agree).has_value());
  CHECK(f.scalar("SELECT agree FROM note WHERE id = '" + id + "'") == 1);
  f.checkCounts(id);
  REQUIRE(f.svc.voteOnNote("bob", id, VoteValue::Agree).has_value());   // same vote again changes nothing
  CHECK(f.scalar("SELECT agree FROM note WHERE id = '" + id + "'") == 1);
  REQUIRE(f.svc.voteOnNote("bob", id, VoteValue::Disagree).has_value());
  CHECK(f.scalar("SELECT agree FROM note WHERE id = '" + id + "'") == 0);
  CHECK(f.scalar("SELECT disagree FROM note WHERE id = '" + id + "'") == 1);
  f.checkCounts(id);
  const auto mine = f.svc.myVotes("bob", AGENT);
  REQUIRE(mine.has_value());
  CHECK(mine->at(id) == VoteValue::Disagree);
  REQUIRE(f.svc.voteOnNote("bob", id, std::nullopt).has_value());
  CHECK(f.scalar("SELECT disagree FROM note WHERE id = '" + id + "'") == 0);
  CHECK(f.scalar("SELECT COUNT(*) FROM note_vote WHERE note_id = '" + id + "'") == 0);
  REQUIRE(f.svc.voteOnNote("bob", id, std::nullopt).has_value());     // retracting nothing is fine
  f.checkCounts(id);
  CHECK(f.changed.size() == 5);
}

TEST_CASE("you cannot vote on your own Note", "[integration][notes]") {
  Fixture f;
  const auto id = *f.svc.createNote("alice", f.note());
  f.changed.clear();
  const auto r = f.svc.voteOnNote("alice", id, VoteValue::Agree);
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().code == GameErrorCode::Forbidden);
  CHECK(f.scalar("SELECT COUNT(*) FROM note_vote") == 0);
  CHECK(f.scalar("SELECT agree FROM note WHERE id = '" + id + "'") == 0);
  CHECK(f.changed.empty());
}

TEST_CASE("hidden or missing Notes can't be voted on", "[integration][notes]") {
  Fixture f;
  const auto id = *f.svc.createNote("alice", f.note());
  f.db.exec("UPDATE note SET hidden = 1 WHERE id = '" + id + "'");
  CHECK(f.svc.voteOnNote("bob", id, VoteValue::Agree).error().code == GameErrorCode::NotFound);
  CHECK(f.svc.voteOnNote("bob", "nope", VoteValue::Agree).error().code == GameErrorCode::NotFound);
  CHECK(f.scalar("SELECT COUNT(*) FROM note_vote") == 0);
}

TEST_CASE("votes reach the card after the cache refreshes, and seeded Notes can be voted on", "[integration][notes]") {
  Fixture f;
  const auto before = f.codex.notes(AGENT, NoteKind::Boon);
  REQUIRE(before.has_value());
  REQUIRE_FALSE(before->empty());
  const auto target = before->front();
  REQUIRE(f.svc.voteOnNote("bob", target.id, VoteValue::Agree).has_value());
  const auto after = f.codex.notes(AGENT, NoteKind::Boon);
  const auto it = std::find_if(after->begin(), after->end(), [&](const NoteView& v) { return v.id == target.id; });
  REQUIRE(it != after->end());
  CHECK(it->agree == target.agree + 1);
}
