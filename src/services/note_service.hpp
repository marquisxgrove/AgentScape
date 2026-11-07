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
#include "services/codex_service.hpp"

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
      : db_(db), clock_(clock), rng_(rng), bus_(bus) {}

  // Attaches a Note to an approved Agent. Text is validated (10..280 code points, plain text); at most
  // NOTES_PER_HOUR Notes per user per rolling hour. Returns the new Note's id.
  Result<std::string> createNote(const std::string& userId, const NewNote& note);

  // Sets, changes (value) or retracts (nullopt) the user's single vote on a Note. Hidden Notes can't be
  // voted on and a user can't vote on their own Note.
  Result<void> voteOnNote(const std::string& userId, const std::string& noteId, std::optional<domain::VoteValue> value);

  // The user's current votes on the visible Notes of one Agent, by Note id.
  Result<std::map<std::string, domain::VoteValue>> myVotes(const std::string& userId, const std::string& agentId);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  app::EventBus& bus_;
};

}  // namespace as::services
