#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "services/activity_service.hpp"
#include "services/codex_store.hpp"
#include "services/xp_service.hpp"

namespace as::services {

enum class ReportTarget { Note, Agent, QuestLog };

struct ReportView {
  std::string id;
  ReportTarget target = ReportTarget::Note;
  std::string targetId;
  std::string targetText;     // the Note's text, or the Agent's name (plain text)
  std::string agentId;        // the Agent the target belongs to
  std::string agentName;
  bool targetHidden = false;  // Notes only
  std::string reporterName;
  std::string reason;
  std::chrono::sys_seconds createdAt;
};

struct ProposalView {
  Agent agent;
  std::string proposerName;
};

struct HiddenNoteView {
  std::string id;
  std::string agentName;
  std::string text;
  std::string authorName;
};

struct KeeperQueue {
  std::vector<ProposalView> proposals;      // PENDING, oldest first
  std::vector<ReportView> reports;          // unresolved, oldest first
  std::vector<HiddenNoteView> hiddenNotes;  // so a Keeper can bring one back
};

inline constexpr std::size_t MIN_REPORT_REASON = 3;
inline constexpr std::size_t MAX_REPORT_REASON = 200;

// Reporting and the Codex Keeper's local moderation tools (agent-codex Requirement 8). Every command is one
// transaction; events are published AFTER commit. Keeper-only commands return Forbidden with the game-voice
// message "Only a Codex Keeper can do that." when the user does not hold the KEEPER role.
class KeeperService {
 public:
  KeeperService(db::Database& db, const core::Clock& clock, core::Rng& rng, app::EventBus& bus)
      : db_(db), clock_(clock), rng_(rng), bus_(bus), activity_(db, clock, bus), xp_(db, clock, rng, activity_) {}
  // xp_ refers to activity_, a sibling member, so a defaulted move would leave it pointing into the moved-from
  // object (the App builds its service bundle as a temporary and moves it). Rebuild both instead.
  KeeperService(KeeperService&& other) noexcept : KeeperService(other.db_, other.clock_, other.rng_, other.bus_) {}
  KeeperService(const KeeperService&) = delete;
  KeeperService& operator=(const KeeperService&) = delete;
  KeeperService& operator=(KeeperService&&) = delete;

  // Turns the Keeper tools on or off for the user (KEEPER / USER). NPCs can't hold the role.
  Result<void> setKeeper(const std::string& userId, bool on);
  Result<bool> isKeeper(const std::string& userId);

  // Anyone can report a visible Note, a visible Quest Log or an approved Agent once. The reason is plain text, 3..200 characters.
  Result<void> reportContent(const std::string& userId, ReportTarget target, const std::string& targetId,
                             const std::string& reason);

  Result<KeeperQueue> queue(const std::string& keeperId);

  // PENDING -> APPROVED or REJECTED. An approval posts an AgentApproved ActivityEvent (after commit).
  Result<void> moderateAgent(const std::string& keeperId, const std::string& agentId, bool approve);
  Result<void> setNoteHidden(const std::string& keeperId, const std::string& noteId, bool hidden);
  Result<void> resolveReport(const std::string& keeperId, const std::string& reportId);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  app::EventBus& bus_;
  ActivityService activity_;   // its own writer: it only needs the db, clock and bus
  XpService xp_;               // an approved proposal earns its proposer 500 XP
};

}  // namespace as::services
