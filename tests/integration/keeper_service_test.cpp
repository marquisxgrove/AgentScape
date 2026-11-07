#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "services/agent_service.hpp"
#include "services/codex_cache.hpp"
#include "services/codex_service.hpp"
#include "services/keeper_service.hpp"
#include "services/note_service.hpp"

using namespace as;
using namespace as::services;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};
const std::string AGENT = "seed:agent:gryphon";

struct Fixture {
  db::Database db;
  core::FixedClock clock{NOW};
  core::SplitMix64 rng{5};
  app::EventBus bus;
  CodexCache cache;
  CodexService codex;
  KeeperService keeper;
  NoteService notes;
  AgentService agents;
  std::vector<std::string> agentEvents, noteEvents, feed;
  std::vector<app::Subscription> subs;

  Fixture()
      : db(open()), cache(bus), codex(db, cache), keeper(db, clock, rng, bus),
        notes(db, clock, rng, bus), agents(db, clock, rng, bus) {
    user("alice", "PLAYER");
    user("bob", "PLAYER");
    user("kay", "PLAYER");
    REQUIRE(keeper.setKeeper("kay", true).has_value());
    subs.push_back(app::Subscription{bus, bus.subscribe<app::AgentChanged>([this](const app::AgentChanged& e) { agentEvents.push_back(e.agentId); })});
    subs.push_back(app::Subscription{bus, bus.subscribe<app::NotesChanged>([this](const app::NotesChanged& e) { noteEvents.push_back(e.agentId); })});
    subs.push_back(app::Subscription{bus, bus.subscribe<app::ActivityPosted>([this](const app::ActivityPosted& e) { feed.push_back(e.message.text); })});
  }
  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, NOW).has_value());
    return std::move(*d);
  }
  void user(const std::string& id, const std::string& kind) {
    db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('" + id + "', '" + id + "', '" + kind + "', 't', 't')");
  }
  std::int64_t scalar(const std::string& sql) {
    auto st = db.prepare(sql);
    REQUIRE(st.step());
    return st.columnInt(0);
  }
  std::string proposal(const std::string& name = "Lumen Scribe") {
    domain::AgentInput in;
    in.name = name;
    in.vendor = "Candlewick";
    in.examine = "A tiny helper that glows when it is unsure.";
    auto id = agents.proposeAgent("alice", in);
    REQUIRE(id.has_value());
    agentEvents.clear();
    return *id;
  }
  std::string note(const std::string& text = "Writes tidy code with sensible names.") {
    NewNote n;
    n.agentId = AGENT;
    n.text = text;
    auto id = notes.createNote("alice", n);
    REQUIRE(id.has_value());
    noteEvents.clear();
    return *id;
  }
};

bool notForKeepers(const Result<void>& r) {
  return !r.has_value() && r.error().code == GameErrorCode::Forbidden && r.error().message == "Only a Codex Keeper can do that.";
}
}  // namespace

TEST_CASE("the Keeper role can be turned on and off, and an NPC can't hold it", "[integration][keeper]") {
  Fixture f;
  CHECK(*f.keeper.isKeeper("kay"));
  CHECK_FALSE(*f.keeper.isKeeper("alice"));
  REQUIRE(f.keeper.setKeeper("kay", false).has_value());
  CHECK_FALSE(*f.keeper.isKeeper("kay"));
  CHECK_FALSE(f.keeper.setKeeper("seed:npc:mossbeard", true).has_value());
  CHECK_FALSE(f.keeper.setKeeper("nobody", true).has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM user WHERE role = 'KEEPER'") == 0);
}

TEST_CASE("every Keeper command refuses a user without the role, and changes nothing", "[integration][keeper]") {
  Fixture f;
  const auto pending = f.proposal();
  const auto noteId = f.note();
  REQUIRE(f.keeper.reportContent("bob", ReportTarget::Note, noteId, "rude").has_value());
  const auto reportId = [&] { auto st = f.db.prepare("SELECT id FROM report"); REQUIRE(st.step()); return st.columnText(0); }();

  CHECK(notForKeepers(f.keeper.moderateAgent("alice", pending, true)));
  CHECK(notForKeepers(f.keeper.setNoteHidden("alice", noteId, true)));
  CHECK(notForKeepers(f.keeper.resolveReport("alice", reportId)));
  const auto q = f.keeper.queue("alice");
  REQUIRE_FALSE(q.has_value());
  CHECK(q.error().message == "Only a Codex Keeper can do that.");

  CHECK(f.scalar("SELECT COUNT(*) FROM agent WHERE id = '" + pending + "' AND status = 'PENDING'") == 1);
  CHECK(f.scalar("SELECT hidden FROM note WHERE id = '" + noteId + "'") == 0);
  CHECK(f.scalar("SELECT resolved FROM report") == 0);
  CHECK(f.agentEvents.empty());
  CHECK(f.feed.empty());
}

TEST_CASE("approving a proposal puts it in the Codex and posts to the feed", "[integration][keeper]") {
  Fixture f;
  const auto id = f.proposal("Lumen Scribe");
  CHECK_FALSE(f.codex.card("lumen-scribe", std::nullopt)->has_value());
  REQUIRE(f.keeper.moderateAgent("kay", id, true).has_value());
  CHECK(f.codex.card("lumen-scribe", std::nullopt)->has_value());
  CHECK(f.agentEvents == std::vector<std::string>{id});
  REQUIRE(f.feed.size() == 1);
  CHECK(f.feed[0] == "Lumen Scribe has joined the Grand Codex.");
  CHECK(f.scalar("SELECT COUNT(*) FROM activity_event WHERE kind = 'AGENT_APPROVED'") == 1);
}

TEST_CASE("rejecting keeps it out of the Codex and posts nothing", "[integration][keeper]") {
  Fixture f;
  const auto id = f.proposal();
  REQUIRE(f.keeper.moderateAgent("kay", id, false).has_value());
  CHECK_FALSE(f.codex.card("lumen-scribe", std::nullopt)->has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM agent WHERE id = '" + id + "' AND status = 'REJECTED'") == 1);
  CHECK(f.feed.empty());
  CHECK(f.agentEvents == std::vector<std::string>{id});
}

TEST_CASE("a decided or unknown proposal can't be decided again", "[integration][keeper]") {
  Fixture f;
  const auto id = f.proposal();
  REQUIRE(f.keeper.moderateAgent("kay", id, true).has_value());
  f.agentEvents.clear();
  const auto again = f.keeper.moderateAgent("kay", id, false);
  REQUIRE_FALSE(again.has_value());
  CHECK(again.error().code == GameErrorCode::NotFound);
  CHECK(f.scalar("SELECT COUNT(*) FROM agent WHERE id = '" + id + "' AND status = 'APPROVED'") == 1);
  CHECK_FALSE(f.keeper.moderateAgent("kay", AGENT, false).has_value());   // a seed Agent is not pending
  CHECK(f.scalar("SELECT COUNT(*) FROM agent WHERE id = '" + AGENT + "' AND status = 'APPROVED'") == 1);
  CHECK(f.scalar("SELECT COUNT(*) FROM activity_event") == 1);
  CHECK(f.agentEvents.empty());
}

TEST_CASE("a report needs a reason, a real target, and can be made once per user", "[integration][keeper]") {
  Fixture f;
  const auto noteId = f.note();
  CHECK_FALSE(f.keeper.reportContent("bob", ReportTarget::Note, noteId, "  ").has_value());
  CHECK_FALSE(f.keeper.reportContent("bob", ReportTarget::Note, noteId, "no").has_value());
  CHECK_FALSE(f.keeper.reportContent("bob", ReportTarget::Note, noteId, std::string(201, 'x')).has_value());
  CHECK_FALSE(f.keeper.reportContent("bob", ReportTarget::Note, noteId, "line\nbreak").has_value());
  CHECK(f.keeper.reportContent("bob", ReportTarget::Note, "missing", "spam").error().code == GameErrorCode::NotFound);
  CHECK(f.keeper.reportContent("bob", ReportTarget::Agent, "missing", "spam").error().code == GameErrorCode::NotFound);
  CHECK(f.keeper.reportContent("ghost", ReportTarget::Note, noteId, "spam").error().code == GameErrorCode::Forbidden);
  CHECK(f.scalar("SELECT COUNT(*) FROM report") == 0);

  REQUIRE(f.keeper.reportContent("bob", ReportTarget::Note, noteId, "  it is spam  ").has_value());
  const auto dup = f.keeper.reportContent("bob", ReportTarget::Note, noteId, "still spam");
  REQUIRE_FALSE(dup.has_value());
  CHECK(dup.error().message == "You have already reported that.");
  CHECK(f.keeper.reportContent("kay", ReportTarget::Note, noteId, "also spam").has_value());   // others may report it
  REQUIRE(f.keeper.reportContent("bob", ReportTarget::Agent, AGENT, "wrong vendor").has_value());
  CHECK(f.scalar("SELECT COUNT(*) FROM report") == 3);
  CHECK(f.scalar("SELECT COUNT(*) FROM report WHERE reason = 'it is spam'") == 1);   // trimmed
}

TEST_CASE("a pending Agent can't be reported", "[integration][keeper]") {
  Fixture f;
  const auto id = f.proposal();
  CHECK(f.keeper.reportContent("bob", ReportTarget::Agent, id, "spam").error().code == GameErrorCode::NotFound);
}

TEST_CASE("a hidden Note leaves the card and can't be voted on, and comes back when unhidden", "[integration][keeper]") {
  Fixture f;
  const auto noteId = f.note("A very visible remark about this familiar.");
  const auto visible = [&] {
    const auto list = f.codex.notes(AGENT, NoteKind::Boon);
    return std::any_of(list->begin(), list->end(), [&](const NoteView& n) { return n.id == noteId; });
  };
  REQUIRE(visible());
  REQUIRE(f.keeper.setNoteHidden("kay", noteId, true).has_value());
  CHECK(f.noteEvents == std::vector<std::string>{AGENT});
  CHECK_FALSE(visible());
  const auto card = f.codex.card("gryphon", std::nullopt, 100);
  CHECK(std::none_of(card.value()->boons.begin(), card.value()->boons.end(), [&](const NoteView& n) { return n.id == noteId; }));
  CHECK(f.notes.voteOnNote("bob", noteId, domain::VoteValue::Agree).error().code == GameErrorCode::NotFound);
  CHECK(f.keeper.reportContent("bob", ReportTarget::Note, noteId, "spam").error().code == GameErrorCode::NotFound);

  REQUIRE(f.keeper.setNoteHidden("kay", noteId, false).has_value());
  CHECK(visible());
  CHECK_FALSE(f.keeper.setNoteHidden("kay", "missing", true).has_value());
}

TEST_CASE("the queue lists proposals, unresolved reports and hidden Notes; resolving removes a report", "[integration][keeper]") {
  Fixture f;
  const auto pending = f.proposal("Lumen Scribe");
  const auto noteId = f.note("Something a reader may dislike here.");
  REQUIRE(f.keeper.reportContent("bob", ReportTarget::Note, noteId, "mean").has_value());
  REQUIRE(f.keeper.reportContent("bob", ReportTarget::Agent, AGENT, "bad info").has_value());
  const auto hiddenId = f.note("Another remark that gets hidden soon.");
  REQUIRE(f.keeper.setNoteHidden("kay", hiddenId, true).has_value());

  auto q = f.keeper.queue("kay");
  REQUIRE(q.has_value());
  REQUIRE(q->proposals.size() == 1);
  CHECK(q->proposals[0].agent.id == pending);
  CHECK(q->proposals[0].proposerName == "alice");
  REQUIRE(q->reports.size() == 2);
  const auto noteReport = std::find_if(q->reports.begin(), q->reports.end(), [](const ReportView& r) { return r.target == ReportTarget::Note; });
  REQUIRE(noteReport != q->reports.end());
  CHECK(noteReport->targetText == "Something a reader may dislike here.");
  CHECK(noteReport->reporterName == "bob");
  CHECK(noteReport->reason == "mean");
  CHECK(noteReport->agentName == "Gryphon");
  CHECK_FALSE(noteReport->targetHidden);
  REQUIRE(q->hiddenNotes.size() == 1);
  CHECK(q->hiddenNotes[0].id == hiddenId);

  REQUIRE(f.keeper.resolveReport("kay", noteReport->id).has_value());
  CHECK(f.keeper.resolveReport("kay", noteReport->id).error().code == GameErrorCode::NotFound);
  q = f.keeper.queue("kay");
  CHECK(q->reports.size() == 1);
  CHECK(f.scalar("SELECT COUNT(*) FROM report") == 2);   // resolved, not deleted
}
