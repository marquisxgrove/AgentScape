#include "services/note_service.hpp"

#include <algorithm>
#include <tuple>
#include <vector>

#include "core/civil_time.hpp"
#include "core/ids.hpp"
#include "domain/codex_validation.hpp"
#include "domain/rate_limit.hpp"

namespace as::services {

namespace {
// Runs fn (which returns a Result and may throw DbError) and turns exceptions into StorageFailed.
template <typename Fn>
auto safely(Fn&& fn) -> decltype(fn()) {
  try {
    return fn();
  } catch (const std::exception& e) {
    return makeError(GameErrorCode::StorageFailed, "Something went wrong. Nothing was saved.", e.what());
  }
}

constexpr const char* MSG_RATE = "You're doing that too often. Rest a while, adventurer.";
constexpr const char* MSG_GONE = "That is no longer there.";
}  // namespace

Result<std::string> NoteService::createNote(const std::string& userId, const NewNote& in) {
  auto text = domain::validateNoteText(in.text);
  if (!text) return makeError(GameErrorCode::InvalidInput, text.error().message);

  std::string version;
  {
    const auto b = in.version.find_first_not_of(" \t");
    if (b != std::string::npos) version = in.version.substr(b, in.version.find_last_not_of(" \t") - b + 1);
    const auto len = domain::utf8Length(version);
    if (!len || *len > MAX_VERSION) {
      return makeError(GameErrorCode::InvalidInput, "That version is too long.");
    }
    for (const char c : version) {
      if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
        return makeError(GameErrorCode::InvalidInput, "That version has characters the scribes can't write.");
      }
    }
  }

  std::string agentId = in.agentId;
  XpGrant grantOut;
  auto result = safely([&]() -> Result<std::string> {
    const auto now = clock_.now();
    db::Transaction tx{db_};

    auto agent = db_.prepare("SELECT 1 FROM agent WHERE id = ? AND status = 'APPROVED'");
    agent.bind(1, in.agentId);
    if (!agent.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);

    auto user = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind <> 'NPC'");
    user.bind(1, userId);
    if (!user.step()) return makeError(GameErrorCode::Forbidden, "You can't do that right now.");

    // The limiter's history is this user's own Notes from the last hour; a new Note is the recorded attempt.
    std::vector<std::chrono::sys_seconds> history;
    auto recent = db_.prepare("SELECT created_at FROM note WHERE author_id = ? AND created_at > ?");
    recent.bind(1, userId).bind(2, core::formatIso8601(now - std::chrono::hours{1}));
    while (recent.step()) {
      if (const auto t = core::parseIso8601(recent.columnText(0))) history.push_back(*t);
    }
    if (!domain::isAllowed(history, now, NOTES_PER_HOUR, std::chrono::hours{1})) {
      return makeError(GameErrorCode::RateLimited, MSG_RATE);
    }

    const std::string id = core::newUlid(now, rng_);
    db_.prepare("INSERT INTO note(id, agent_id, author_id, kind, skill, version, text, created_at) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
        .bind(1, id)
        .bind(2, in.agentId)
        .bind(3, userId)
        .bind(4, noteKindToDb(in.kind))
        .bind(5, in.skill ? std::optional<std::string>{domain::storageName(domain::StatKey{*in.skill})} : std::nullopt)
        .bind(6, version.empty() ? std::nullopt : std::optional<std::string>{version})
        .bind(7, *text)
        .bind(8, core::formatIso8601(now))
        .run();
    // The Note and its XP commit together (the ledger is unique per Note, so a retry can't pay twice).
    auto grant = xp_.award(userId, domain::XpReason::Note, id, domain::XP_REWARDS.note);
    if (!grant) return tl::unexpected(grant.error());
    tx.commit();
    grantOut = std::move(*grant);
    return id;
  });
  if (result) {
    bus_.publish(app::NotesChanged{agentId});
    publishGrant(bus_, activity_, userId, grantOut);
  }
  return result;
}

Result<void> NoteService::voteOnNote(const std::string& userId, const std::string& noteId,
                                     std::optional<domain::VoteValue> value) {
  std::string agentId;
  auto result = safely([&]() -> Result<void> {
    db::Transaction tx{db_};
    auto note = db_.prepare("SELECT agent_id, author_id FROM note WHERE id = ? AND hidden = 0");
    note.bind(1, noteId);
    if (!note.step()) return makeError(GameErrorCode::NotFound, MSG_GONE);
    agentId = note.columnText(0);
    const std::string authorId = note.columnText(1);

    auto user = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind <> 'NPC'");
    user.bind(1, userId);
    if (!user.step()) return makeError(GameErrorCode::Forbidden, "You can't do that right now.");

    domain::VoteMap before;
    auto old = db_.prepare("SELECT value FROM note_vote WHERE note_id = ? AND user_id = ?");
    old.bind(1, noteId).bind(2, userId);
    if (old.step()) before[userId] = old.columnInt(0) > 0 ? domain::VoteValue::Agree : domain::VoteValue::Disagree;

    auto after = domain::applyVote(before, domain::VoteOp{userId, noteId, value}, authorId);
    if (!after) return makeError(GameErrorCode::Forbidden, "You can't vote on your own words.");

    // Counts move by the difference between the old and new tally, so they always match the vote rows.
    const auto t0 = domain::tally(before);
    const auto t1 = domain::tally(*after);
    if (after->empty()) {
      db_.prepare("DELETE FROM note_vote WHERE note_id = ? AND user_id = ?").bind(1, noteId).bind(2, userId).run();
    } else {
      db_.prepare("INSERT INTO note_vote(note_id, user_id, value) VALUES (?, ?, ?) "
                  "ON CONFLICT(note_id, user_id) DO UPDATE SET value = excluded.value")
          .bind(1, noteId)
          .bind(2, userId)
          .bind(3, static_cast<int>(after->at(userId)))
          .run();
    }
    db_.prepare("UPDATE note SET agree = agree + ?, disagree = disagree + ? WHERE id = ?")
        .bind(1, t1.agree - t0.agree)
        .bind(2, t1.disagree - t0.disagree)
        .bind(3, noteId)
        .run();
    tx.commit();
    return {};
  });
  if (result) bus_.publish(app::NotesChanged{agentId});
  return result;
}

Result<void> NoteService::retractAllVotes(const std::string& userId) {
  std::vector<std::string> agents;
  auto result = safely([&]() -> Result<void> {
    db::Transaction tx{db_};
    auto votes = db_.prepare("SELECT v.note_id, v.value, n.agent_id FROM note_vote v JOIN note n ON n.id = v.note_id WHERE v.user_id = ?");
    votes.bind(1, userId);
    std::vector<std::tuple<std::string, int, std::string>> rows;
    while (votes.step()) rows.emplace_back(votes.columnText(0), static_cast<int>(votes.columnInt(1)), votes.columnText(2));
    for (const auto& [noteId, value, agentId] : rows) {
      db_.prepare(value > 0 ? "UPDATE note SET agree = agree - 1 WHERE id = ?" : "UPDATE note SET disagree = disagree - 1 WHERE id = ?")
          .bind(1, noteId)
          .run();
      if (std::find(agents.begin(), agents.end(), agentId) == agents.end()) agents.push_back(agentId);
    }
    db_.prepare("DELETE FROM note_vote WHERE user_id = ?").bind(1, userId).run();
    tx.commit();
    return {};
  });
  if (result) {
    for (const auto& a : agents) bus_.publish(app::NotesChanged{a});
  }
  return result;
}

Result<std::map<std::string, domain::VoteValue>> NoteService::myVotes(const std::string& userId,
                                                                      const std::string& agentId) {
  return safely([&]() -> Result<std::map<std::string, domain::VoteValue>> {
    std::map<std::string, domain::VoteValue> out;
    auto st = db_.prepare("SELECT v.note_id, v.value FROM note_vote v JOIN note n ON n.id = v.note_id "
                          "WHERE v.user_id = ? AND n.agent_id = ? AND n.hidden = 0");
    st.bind(1, userId).bind(2, agentId);
    while (st.step()) out[st.columnText(0)] = st.columnInt(1) > 0 ? domain::VoteValue::Agree : domain::VoteValue::Disagree;
    return out;
  });
}

}  // namespace as::services
