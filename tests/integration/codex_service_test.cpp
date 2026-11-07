#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "app/event_bus.hpp"
#include "db/database.hpp"
#include "db/migrations.hpp"
#include "db/seed_loader.hpp"
#include "services/codex_cache.hpp"
#include "domain/voting.hpp"
#include "services/codex_service.hpp"

using namespace as;
using namespace as::services;
using domain::ListQuery;
using domain::QuestType;
using domain::SortKey;

namespace {
const std::chrono::sys_seconds NOW{std::chrono::seconds{1793828551}};

struct Fixture {
  db::Database db;
  app::EventBus bus;
  CodexCache cache;
  CodexService svc;

  Fixture() : db(open()), cache(bus), svc(db, cache) {}

  static db::Database open() {
    auto d = db::Database::open(":memory:");
    REQUIRE(d.has_value());
    REQUIRE(db::applyMigrations(*d).has_value());
    REQUIRE(db::runSeed(*d, NOW).has_value());
    return std::move(*d);
  }

  std::int64_t scalar(std::string_view sql) {
    auto st = db.prepare(sql);
    REQUIRE(st.step());
    return st.columnInt(0);
  }
};

std::vector<std::string> slugs(const CodexPage& p) {
  std::vector<std::string> out;
  for (const auto& r : p.rows) out.push_back(r.agent.slug);
  return out;
}
}  // namespace

TEST_CASE("the list shows every approved Agent with a Total level and a Quest Log count", "[integration][codex-service]") {
  Fixture f;
  const auto page = f.svc.list({}, std::nullopt);
  REQUIRE(page.has_value());
  CHECK(page->total == 14);
  CHECK(page->rows.size() == 14);
  CHECK(page->slice.pages == 1);
  for (const auto& r : page->rows) {
    CHECK(r.levels.total >= 10);
    CHECK(r.levels.overallCount > 0);                         // every seeded Agent has Quest Logs
    CHECK(r.agent.status == AgentStatus::Approved);
  }
  // Default order: highest Total level first.
  for (std::size_t i = 0; i + 1 < page->rows.size(); ++i) {
    CHECK(page->rows[i].levels.total >= page->rows[i + 1].levels.total);
  }
  CHECK(page->vendors.size() >= 10);
  CHECK(std::is_sorted(page->vendors.begin(), page->vendors.end(), [](const std::string& a, const std::string& b) {
    std::string la = a, lb = b;
    for (auto& c : la) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (auto& c : lb) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return la < lb;
  }));
}

TEST_CASE("only approved Agents are listed or have a card", "[integration][codex-service]") {
  Fixture f;
  f.db.exec("INSERT INTO agent(id, slug, name, vendor, kind, examine, status, created_at, updated_at) "
            "VALUES ('p1', 'pending-thing', 'Pending Thing', 'V', 'TOOL', 'Waiting.', 'PENDING', 't', 't'),"
            "       ('r1', 'rejected-thing', 'Rejected Thing', 'V', 'TOOL', 'Nope.', 'REJECTED', 't', 't')");
  f.bus.publish(app::AgentChanged{"p1"});
  const auto page = f.svc.list({}, std::nullopt);
  CHECK(page->total == 14);
  CHECK(f.svc.card("pending-thing", std::nullopt)->has_value() == false);
  CHECK(f.svc.card("rejected-thing", std::nullopt)->has_value() == false);
  CHECK(f.svc.card("spellforge", std::nullopt)->has_value());
  CHECK_FALSE(f.svc.card("no-such-agent", std::nullopt)->has_value());
}

TEST_CASE("search, filters and sorting reach the service", "[integration][codex-service]") {
  Fixture f;
  ListQuery q;
  q.text = "lanternfold";
  auto page = f.svc.list(q, std::nullopt);
  auto got = slugs(*page);
  std::sort(got.begin(), got.end());
  CHECK(got == std::vector<std::string>{"gryphon", "gryphon-chat"});

  q = {};
  q.kind = domain::AgentKind::Tool;
  CHECK(f.svc.list(q, std::nullopt)->total == 6);
  q.kind = domain::AgentKind::Model;
  CHECK(f.svc.list(q, std::nullopt)->total == 8);

  q = {};
  q.sort = SortKey::Name;
  page = f.svc.list(q, std::nullopt);
  CHECK(page->rows.front().agent.name == "Cairn");
  CHECK(page->rows.back().agent.name == "Twinwick");

  q = {};
  q.sort = SortKey::Skill;
  q.skill = domain::Skill::Speed;
  page = f.svc.list(q, std::nullopt);
  for (std::size_t i = 0; i + 1 < page->rows.size(); ++i) {
    const auto a = page->rows[i].levels.skills[8];
    const auto b = page->rows[i + 1].levels.skills[8];
    if (a && b) CHECK(*a >= *b);
    if (!a) CHECK_FALSE(b.has_value());   // once unranked, always unranked after
  }
}

TEST_CASE("picking a Quest Type scopes every displayed level", "[integration][codex-service]") {
  Fixture f;
  const auto all = f.svc.list({}, std::nullopt);
  const auto coding = f.svc.list({}, QuestType::Coding);
  REQUIRE(all.has_value());
  REQUIRE(coding.has_value());
  bool anyDifferent = false;
  for (const auto& r : coding->rows) {
    const auto same = std::find_if(all->rows.begin(), all->rows.end(), [&](const CodexRow& x) { return x.agent.id == r.agent.id; });
    REQUIRE(same != all->rows.end());
    if (same->levels.total != r.levels.total || same->levels.overallCount != r.levels.overallCount) anyDifferent = true;
    CHECK(r.levels.overallCount <= same->levels.overallCount);   // a scope never has more Quest Logs than "all"
  }
  CHECK(anyDifferent);
}

TEST_CASE("pages hold 20 Agents each", "[integration][codex-service]") {
  Fixture f;
  for (int i = 0; i < 31; ++i) {
    f.db.prepare("INSERT INTO agent(id, slug, name, vendor, kind, examine, status, created_at, updated_at) "
                 "VALUES (?, ?, ?, 'Bulk', 'TOOL', 'Filler.', 'APPROVED', 't', 't')")
        .bind(1, "bulk-" + std::to_string(i))
        .bind(2, "bulk-" + std::to_string(i))
        .bind(3, "Bulk " + std::to_string(100 + i))
        .run();
  }
  f.bus.publish(app::AgentChanged{"bulk"});
  ListQuery q;
  q.sort = SortKey::Name;
  const auto first = f.svc.list(q, std::nullopt);
  CHECK(first->total == 45);
  CHECK(first->slice.pages == 3);
  CHECK(first->rows.size() == 20);
  q.page = 2;
  const auto last = f.svc.list(q, std::nullopt);
  CHECK(last->rows.size() == 5);
  q.page = 99;                                      // out of range: clamped to the last page
  CHECK(f.svc.list(q, std::nullopt)->slice.page == 2);
}

TEST_CASE("the examine card ranks Boons and Banes by Wilson score, then newest", "[integration][codex-service]") {
  Fixture f;
  f.db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('u1', 'Ann', 'PLAYER', 't', 't')");
  f.db.exec("DELETE FROM note WHERE agent_id = 'seed:agent:spellforge'");
  const auto add = [&](const std::string& id, int agree, int disagree, const std::string& at, int hidden = 0) {
    f.db.prepare("INSERT INTO note(id, agent_id, author_id, kind, text, agree, disagree, hidden, created_at) "
                 "VALUES (?, 'seed:agent:spellforge', 'u1', 'BOON', ?, ?, ?, ?, ?)")
        .bind(1, id).bind(2, "Note text for " + id).bind(3, agree).bind(4, disagree).bind(5, hidden).bind(6, at).run();
  };
  add("few-votes", 2, 0, "2026-11-01T00:00:00Z");        // perfect ratio, but only 2 votes
  add("many-votes", 200, 10, "2026-10-01T00:00:00Z");     // strong evidence
  add("same-old", 8, 2, "2026-10-01T00:00:00Z");
  add("same-new", 8, 2, "2026-11-02T00:00:00Z");          // identical score, newer
  add("hidden-star", 900, 0, "2026-11-03T00:00:00Z", 1);  // hidden: must never appear
  f.bus.publish(app::NotesChanged{"seed:agent:spellforge"});

  const auto card = f.svc.card("spellforge", std::nullopt);
  REQUIRE(card->has_value());
  const auto& boons = (*card)->boons;
  REQUIRE(boons.size() == 4);
  CHECK(boons[0].id == "many-votes");    // 2 votes can't outrank 200
  CHECK(boons[1].id == "same-new");      // equal score: newest first
  CHECK(boons[2].id == "same-old");
  CHECK(boons[3].id == "few-votes");
  CHECK((*card)->boonTotal == 4);
  for (const auto& b : boons) CHECK(b.id != "hidden-star");
  CHECK(boons[0].score > boons[3].score);
  CHECK(boons[0].authorName == "Ann");
}

TEST_CASE("the card shows only the top five of each, but reports how many exist", "[integration][codex-service]") {
  Fixture f;
  f.db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('u1', 'Ann', 'PLAYER', 't', 't')");
  for (int i = 0; i < 9; ++i) {
    f.db.prepare("INSERT INTO note(id, agent_id, author_id, kind, text, agree, disagree, created_at) "
                 "VALUES (?, 'seed:agent:quillback', 'u1', 'BANE', ?, ?, 0, 't')")
        .bind(1, "extra-" + std::to_string(i)).bind(2, "An extra bane number " + std::to_string(i)).bind(3, 10 + i).run();
  }
  f.bus.publish(app::NotesChanged{"seed:agent:quillback"});
  const auto card = f.svc.card("quillback", std::nullopt);
  REQUIRE(card->has_value());
  CHECK((*card)->banes.size() == 5);
  CHECK((*card)->baneTotal >= 9 + 1);                       // the seeded Bane(s) plus nine more
  CHECK((*card)->banes.front().id == "extra-8");            // most agreed first
  CHECK(f.svc.notes("seed:agent:quillback", NoteKind::Bane)->size() == (*card)->baneTotal);   // "view all"
  CHECK(f.svc.notes("seed:agent:quillback", NoteKind::Bane, 3)->size() == 3);
}

TEST_CASE("the card's levels follow the chosen Quest Type", "[integration][codex-service]") {
  Fixture f;
  const auto all = f.svc.card("gryphon", std::nullopt);
  const auto debug = f.svc.card("gryphon", QuestType::Debugging);
  REQUIRE(all->has_value());
  REQUIRE(debug->has_value());
  CHECK((*all)->levels.overallCount > (*debug)->levels.overallCount);
  CHECK((*all)->levels.overall.has_value());
}

TEST_CASE("the list refreshes after a rating event, and not before", "[integration][codex-service]") {
  Fixture f;
  const auto before = f.svc.list({}, std::nullopt)->rows.front();
  {
    db::Transaction tx{f.db};
    // A flood of 1-star Overall ratings on the current leader drags its level down.
    const RatingDelta d[] = {{domain::StatKey{domain::Overall{}}, 1, 1}};
    for (int i = 0; i < 300; ++i) REQUIRE(applyRatingDelta(f.db, before.agent.id, QuestType::Other, d).has_value());
    tx.commit();
  }
  // Cached until the event is published (the contract: services publish after commit).
  CHECK(f.svc.list({}, std::nullopt)->rows.front().levels.overallCount == before.levels.overallCount);
  f.bus.publish(app::RatingsChanged{before.agent.id});
  const auto after = f.svc.list({}, std::nullopt);
  const auto it = std::find_if(after->rows.begin(), after->rows.end(), [&](const CodexRow& r) { return r.agent.id == before.agent.id; });
  REQUIRE(it != after->rows.end());
  CHECK(it->levels.overallCount == before.levels.overallCount + 300);
  CHECK(*it->levels.overall < *before.levels.overall);
}

TEST_CASE("the graveyard holds the top visible Banes of approved Agents", "[integration][codex-service][graveyard]") {
  Fixture f;
  const auto stones = f.svc.tombstones();
  REQUIRE(stones.has_value());
  REQUIRE_FALSE(stones->empty());
  CHECK(stones->size() <= 13);
  for (const auto& t : *stones) {
    CHECK(t.agree + t.disagree >= 3);
    CHECK_FALSE(t.text.empty());
    CHECK_FALSE(t.agentName.empty());
    CHECK(f.scalar("SELECT COUNT(*) FROM note WHERE id = '" + t.noteId + "' AND kind = 'BANE' AND hidden = 0") == 1);
  }
  // Best first: the first stone is at least as well supported as the last.
  const auto score = [](const Tombstone& t) { return domain::wilsonLowerBound(t.agree, t.disagree); };
  for (std::size_t i = 0; i + 1 < stones->size(); ++i) CHECK(score((*stones)[i]) >= score((*stones)[i + 1]));
}

TEST_CASE("hiding a Bane, or losing its Agent, takes its tombstone away", "[integration][codex-service][graveyard]") {
  Fixture f;
  const auto before = f.svc.tombstones();
  REQUIRE(before.has_value());
  const auto first = before->front();
  f.db.exec("UPDATE note SET hidden = 1 WHERE id = '" + first.noteId + "'");
  auto after = f.svc.tombstones();
  CHECK(std::none_of(after->begin(), after->end(), [&](const Tombstone& t) { return t.noteId == first.noteId; }));

  const auto second = after->front();
  f.db.exec("UPDATE agent SET status = 'REJECTED' WHERE slug = '" + second.agentSlug + "'");
  after = f.svc.tombstones();
  CHECK(std::none_of(after->begin(), after->end(), [&](const Tombstone& t) { return t.agentSlug == second.agentSlug; }));
}

TEST_CASE("a Bane under three votes has no tombstone, and a Boon never does", "[integration][codex-service][graveyard]") {
  Fixture f;
  f.db.exec("INSERT INTO user(id, name, kind, created_at, last_seen_at) VALUES ('u1', 'u1', 'PLAYER', 't', 't')");
  f.db.exec("INSERT INTO note(id, agent_id, author_id, kind, text, agree, disagree, hidden, created_at) VALUES "
            "('few', 'seed:agent:quillback', 'u1', 'BANE', 'Two votes only here.', 2, 0, 0, 't'), "
            "('boon', 'seed:agent:quillback', 'u1', 'BOON', 'Lots of votes for a Boon.', 500, 0, 0, 't')");
  const auto stones = f.svc.tombstones();
  CHECK(std::none_of(stones->begin(), stones->end(), [](const Tombstone& t) { return t.noteId == "few" || t.noteId == "boon"; }));
}
