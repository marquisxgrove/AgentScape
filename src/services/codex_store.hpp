#pragma once

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "db/database.hpp"
#include "domain/codex_validation.hpp"
#include "domain/skills.hpp"
#include "domain/stat_table.hpp"

namespace as::services {

using AgentKind = domain::AgentKind;
enum class AgentStatus { Pending, Approved, Rejected };

struct Agent {
  std::string id;
  std::string slug;
  std::string name;
  std::string vendor;
  AgentKind kind = AgentKind::Model;
  std::string examine;
  std::string about;
  std::optional<std::string> website;  // https only
  AgentStatus status = AgentStatus::Pending;
  std::optional<std::string> proposedById;
  std::chrono::sys_seconds createdAt;
  std::chrono::sys_seconds updatedAt;
};

const char* kindToDb(AgentKind kind);       // "MODEL" / "TOOL"
const char* statusToDb(AgentStatus status); // "PENDING" / "APPROVED" / "REJECTED"
const char* kindDisplay(AgentKind kind);    // "Model" / "Tool"

// Read side. The functions only read; they throw nothing (errors come back as StorageFailed).
Result<std::optional<Agent>> findAgentBySlug(db::Database& db, const std::string& slug);
Result<std::optional<Agent>> findAgentById(db::Database& db, const std::string& id);
Result<std::vector<Agent>> listAgents(db::Database& db, AgentStatus status);  // by name, case-insensitive
Result<domain::StatTable> loadStats(db::Database& db, const std::string& agentId);

// One change to an aggregate. A new rating r is {key, +r, +1}; deleting it is {key, -r, -1}; changing it
// from r to r2 is {key, r2 - r, 0}. Each Quest Log writes one Overall delta plus one per rated Skill.
struct RatingDelta {
  domain::StatKey key;
  std::int64_t sumDelta = 0;
  std::int64_t countDelta = 0;
};

// Adds the deltas to the (agent, stat, quest type) aggregates with an upsert
// (sum = sum + delta, count = count + delta). Must run inside the caller's open transaction so the stat
// change commits atomically with the Quest Log write; the caller publishes RatingsChanged AFTER commit.
// The table's CHECK constraints reject any result that is not a possible set of 1..5 ratings, which
// surfaces a buggy delta as a failed transaction instead of corrupt levels.
Result<void> applyRatingDelta(db::Database& db, const std::string& agentId, domain::QuestType type,
                              std::span<const RatingDelta> deltas);

}  // namespace as::services
