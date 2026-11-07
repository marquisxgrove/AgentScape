#pragma once

#include <optional>
#include <string>

#include "app/event_bus.hpp"
#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/codex_validation.hpp"

namespace as::services {

inline constexpr int PROPOSALS_PER_DAY = 3;  // Requirement 4.3

struct ProposeError {
  GameError error;
  std::string field;                         // which form field to mark, if any
  std::optional<std::string> existingSlug;   // set when the slug is taken by an APPROVED Agent (offer to open it)
};

// Agent proposals (agent-codex Requirement 4.1-4.3, 4.5). One transaction; AgentChanged is published after commit.
class AgentService {
 public:
  AgentService(db::Database& db, const core::Clock& clock, core::Rng& rng, app::EventBus& bus)
      : db_(db), clock_(clock), rng_(rng), bus_(bus) {}

  // Creates a PENDING Agent. The slug comes from the name; a slug that already exists in ANY status is refused.
  // At most PROPOSALS_PER_DAY proposals per user per rolling 24 hours (rejected ones still count).
  // Returns the new Agent's id.
  Result<std::string, ProposeError> proposeAgent(const std::string& userId, domain::AgentInput input);

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  app::EventBus& bus_;
};

}  // namespace as::services
