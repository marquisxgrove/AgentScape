#pragma once

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/codex_query.hpp"
#include "domain/ratings.hpp"
#include "services/activity_service.hpp"
#include "services/adventurer_service.hpp"
#include "services/xp_service.hpp"

namespace as::services {

inline constexpr int QUEST_LOGS_PER_HOUR = 5;   // new Quest Logs per user per rolling hour (Requirement 1.4)

// A stored Quest Log.
struct QuestLogRecord {
  std::string id;
  std::string authorId;
  std::string authorName;
  domain::QuestLogInput input;     // agentId, version, quest type, ratings, text, tags
  std::string agentName;
  std::string agentSlug;           // to open the Agent's card
  std::string snapshot;            // JSON Adventurer_Snapshot, frozen when the log was written
  int helpfulCount = 0;
  bool hidden = false;
  bool excluded = false;
  std::chrono::sys_seconds createdAt;
  std::chrono::sys_seconds updatedAt;
};

inline constexpr std::size_t QUEST_PAGE_SIZE = 12;   // notes per page on the Tavern board (Requirement 3.1)

enum class QuestSort { Newest, MostHelpful };

// What the Tavern board shows: non-hidden Quest Logs of approved Agents, filtered and sorted, one page at a time.
struct QuestListQuery {
  std::optional<std::string> agentId;
  std::optional<domain::QuestType> questType;
  QuestSort sort = QuestSort::Newest;
  std::size_t page = 0;   // zero-based; clamped to the last page
};

struct QuestPage {
  std::vector<QuestLogRecord> logs;          // this page only
  std::size_t total = 0;                     // matches across all pages
  domain::PageSlice slice;
  std::set<std::string> helpedByViewer;      // ids on this page that the viewer has marked Helpful
};

// What the screen turns into chatbox lines, the "+N XP" drop, fireworks and unlock messages.
struct QuestOutcome {
  std::string questLogId;
  int xpGained = 0;
  int levelBefore = 1;
  int levelAfter = 1;
  std::vector<std::string> unlocked;   // item ids newly unlocked
};

// Why a command was refused. `fields` carries every form problem for validation failures; `existingId` is set
// when the user already has a log for this (Agent, version), so the form can open it for editing instead.
struct QuestError {
  GameError error;
  domain::ValidationErrors fields;
  std::optional<std::string> existingId;
};

// Quest Logs (quest-logs Requirements 1 to 5). Every command is ONE transaction: the log, the rating
// aggregates (agent_skill_stat), the XP ledger, unlocks and activity rows commit or roll back together. Events
// are published AFTER commit: RatingsChanged, XpChanged, ItemUnlocked, ActivityPosted.
class QuestService {
 public:
  // Self-contained (it builds the small services it uses from the same db, clock and bus), so it can live in the
  // App's service bundle, which is moved after construction, without any dangling references.
  QuestService(db::Database& db, const core::Clock& clock, core::Rng& rng, app::EventBus& bus)
      : db_(db), clock_(clock), rng_(rng), bus_(bus), activity_(db, clock, bus), adventurers_(db, clock, rng),
        xp_(db, clock, rng, activity_) {}

  // Writes a new Quest Log for an approved Agent, with a snapshot of the active Adventurer. Needs an Adventurer
  // ("You need an Adventurer to go questing."), at most one log per (user, Agent, version), and at most
  // QUEST_LOGS_PER_HOUR new logs per rolling hour. Awards 250 XP and posts "<name> completed a <type> quest with
  // <agent> (n/5)."
  Result<QuestOutcome, QuestError> createQuestLog(const std::string& userId, domain::QuestLogInput input);

  // Edits your own log. The Agent and version identify the log and can't change; the snapshot is kept. The
  // ratings change by the difference. No XP is awarded or taken.
  Result<void, QuestError> updateQuestLog(const std::string& userId, const std::string& questLogId, domain::QuestLogInput input);

  // Deletes your own log: its ratings leave the aggregates and the XP it earned (creation and helpful) is revoked.
  Result<void> deleteQuestLog(const std::string& userId, const std::string& questLogId);

  // Adds or withdraws your Helpful vote (not on your own, not on a hidden log). The author earns 25 XP per vote,
  // up to 250 a UTC day; withdrawing takes that XP back. Returns whether you now have a vote on it.
  Result<bool> toggleHelpful(const std::string& userId, const std::string& questLogId);

  // Keeper only. Hiding takes a log off the board; excluding also takes its ratings out of the aggregates.
  Result<void> setQuestLogModeration(const std::string& keeperId, const std::string& questLogId, bool hidden, bool excluded);

  // For a user who is about to be deleted (an idle Guest): withdraws their Helpful votes on other people's logs
  // (taking back the XP those paid) and deletes their own logs through the normal path, so the aggregates stay
  // equal to a recompute. Returns how many Quest Logs were removed.
  Result<int> purgeUser(const std::string& userId);

  // The Tavern board. Newest = latest first; MostHelpful = most Helpful votes first, then latest. Ties end on the id,
  // so the order is total. Hidden logs and logs of Agents that are no longer approved are left out.
  Result<QuestPage> list(const QuestListQuery& query, const std::string& viewerId);

  // An Agent's most helpful logs, for its examine card (Requirement 3.3), best first.
  Result<std::vector<QuestLogRecord>> topHelpful(const std::string& agentId, std::size_t limit = 5);

  // Whether this user has marked the log Helpful.
  Result<bool> hasVoted(const std::string& userId, const std::string& questLogId);

  Result<std::optional<QuestLogRecord>> get(const std::string& questLogId);
  // The user's own log for this Agent and version, if any.
  Result<std::optional<std::string>> findOwn(const std::string& userId, const std::string& agentId, const std::string& version);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  app::EventBus& bus_;
  ActivityService activity_;   // declared before xp_, which holds a reference to it
  AdventurerService adventurers_;
  XpService xp_;
};

}  // namespace as::services
