#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/compare.hpp"
#include "domain/skills.hpp"

namespace as::services {

struct ProvingSelection {
  std::vector<std::string> slugs;                 // 0..3, no duplicates, in slot order
  std::optional<domain::QuestType> questType;     // nullopt = All
  bool operator==(const ProvingSelection&) const = default;
};

// Remembers each user's last Proving Grounds comparison (agent-codex Requirement 6.3). Stored in its own table
// (migration 0004) rather than in the user's settings JSON: the Settings tab writes that whole JSON back on every
// toggle, which would silently overwrite a comparison saved in between.
class ProvingService {
 public:
  ProvingService(db::Database& db, const core::Clock& clock) : db_(db), clock_(clock) {}

  // Saves the selection. Duplicates are dropped and only the first MAX_COMPARE slugs are kept.
  Result<ProvingSelection> remember(const std::string& userId, ProvingSelection selection);

  // The last saved selection, or an empty one if there is none.
  Result<ProvingSelection> restore(const std::string& userId);

 private:
  db::Database& db_;
  const core::Clock& clock_;
};

}  // namespace as::services
