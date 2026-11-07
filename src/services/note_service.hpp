#pragma once

#include <map>
#include <optional>
#include <string>

#include "app/event_bus.hpp"
#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/skills.hpp"
#include "domain/voting.hpp"
#include "services/activity_service.hpp"
#include "services/codex_service.hpp"
#include "services/xp_service.hpp"

namespace as::services {

inline constexpr int NOTES_PER_HOUR = 10;  // Requirement 5.2
inline constexpr std::size_t MAX_VERSION = 40;

struct NewNote {
  std::string agentId;
  NoteKind kind = NoteKind::Boon;
  std::string text;
  std::optional<domain::Skill> skill;
  std::string version;  // optional; blank = none
};

// Boons and Banes commands (agent-codex Requirement 5). Each command is one transaction and publishes
// NotesChanged AFTER it commits, so the Codex views refresh.
class NoteService {
 public:
  NoteService(db::Database& db, const core::Clock& clock, core::Rng& rng, app::EventBus& bus)
      : db_(db), clock_(clock), rng_(rng), bus_(bus), activity_(db, clock, bus), xp_(db, clock, rng, activity_) {}
  // xp_ refers to activity_, a sibling member, so a defaulted move would leave it pointing into the moved-from
  // object (the App builds its service bundle as a temporary and moves it). Rebuild both instead.
  NoteService(NoteService&& other) noexcept : NoteService(other.db_, other.clock_, other.rng_, other.bus_) {}
  NoteService(const NoteService&) = delete;
  NoteService& operator=(const NoteService&) = delete;
  NoteService& operator=(NoteService&&) = delete;

  // Attaches a Note to an approved Agent. Text is validated (10..280 code points, plain text); at most
  // NOTES_PER_HOUR Notes per user per rolling hour. Returns the new Note's id.
  Result<std::string> createNote(const std::string& userId, const NewNote& note);

  // Sets, changes (value) or retracts (nullopt) the user's single vote on a Note. Hidden Notes can't be
  // voted on and a user can't vote on their own Note.
  Result<void> voteOnNote(const std::string& userId, const std::string& noteId, std::optional<domain::VoteValue> value);

  // For a user who is about to be deleted (an idle Guest): withdraws every vote they cast, correcting the Notes'
  // agree/disagree counts, which ON DELETE CASCADE alone would leave inflated. Works on hidden Notes too.
  Result<void> retractAllVotes(const std::string& userId);

  // The user's current votes on the visible Notes of one Agent, by Note id.
  Result<std::map<std::string, domain::VoteValue>> myVotes(const std::string& userId, const std::string& agentId);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  app::EventBus& bus_;
  ActivityService activity_;   // its own writer, for level milestones; it only needs the db, clock and bus
  XpService xp_;               // a new Note earns 50 XP (quest-logs Requirement 5.1)
};

}  // namespace as::services
