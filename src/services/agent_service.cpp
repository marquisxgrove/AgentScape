#include "services/agent_service.hpp"

#include <vector>

#include "core/civil_time.hpp"
#include "core/ids.hpp"
#include "domain/rate_limit.hpp"
#include "domain/slug.hpp"
#include "services/codex_store.hpp"

namespace as::services {

namespace {
ProposeError plain(GameErrorCode code, std::string msg, std::string field = {}, std::string detail = {}) {
  return ProposeError{GameError{code, std::move(msg), std::move(detail)}, std::move(field), std::nullopt};
}
}  // namespace

Result<std::string, ProposeError> AgentService::proposeAgent(const std::string& userId, domain::AgentInput raw) {
  auto input = domain::validateAgentInput(std::move(raw));
  if (!input) return tl::unexpected(plain(GameErrorCode::InvalidInput, input.error().message, input.error().field));

  const std::string slug = domain::slugify(input->name);
  if (slug.empty()) {
    return tl::unexpected(plain(GameErrorCode::InvalidInput, "Give it a name with letters or numbers in it.", "name"));
  }

  std::string newId;
  Result<std::string, ProposeError> result = [&]() -> Result<std::string, ProposeError> {
    try {
      const auto now = clock_.now();
      db::Transaction tx{db_};

      auto user = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind <> 'NPC'");
      user.bind(1, userId);
      if (!user.step()) return tl::unexpected(plain(GameErrorCode::Forbidden, "You can't do that right now."));

      auto existing = db_.prepare("SELECT status FROM agent WHERE slug = ?");
      existing.bind(1, slug);
      if (existing.step()) {
        const bool approved = existing.columnText(0) == "APPROVED";
        ProposeError e = plain(GameErrorCode::InvalidInput,
                               approved ? "The Codex already holds a familiar by that name."
                                        : "A familiar by that name has already been proposed.",
                               "name");
        if (approved) e.existingSlug = slug;
        return tl::unexpected(std::move(e));
      }

      std::vector<std::chrono::sys_seconds> history;
      auto recent = db_.prepare("SELECT created_at FROM agent WHERE proposed_by_id = ? AND created_at > ?");
      recent.bind(1, userId).bind(2, core::formatIso8601(now - std::chrono::hours{24}));
      while (recent.step()) {
        if (const auto t = core::parseIso8601(recent.columnText(0))) history.push_back(*t);
      }
      if (!domain::isAllowed(history, now, PROPOSALS_PER_DAY, std::chrono::hours{24})) {
        return tl::unexpected(plain(GameErrorCode::RateLimited, "You're doing that too often. Rest a while, adventurer."));
      }

      newId = core::newUlid(now, rng_);
      const std::string stamp = core::formatIso8601(now);
      db_.prepare("INSERT INTO agent(id, slug, name, vendor, kind, examine, about, website, status, proposed_by_id, "
                  "created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'PENDING', ?, ?, ?)")
          .bind(1, newId)
          .bind(2, slug)
          .bind(3, input->name)
          .bind(4, input->vendor)
          .bind(5, kindToDb(input->kind))
          .bind(6, input->examine)
          .bind(7, input->about)
          .bind(8, input->website)
          .bind(9, userId)
          .bind(10, stamp)
          .bind(11, stamp)
          .run();
      tx.commit();
      return newId;
    } catch (const std::exception& e) {
      return tl::unexpected(plain(GameErrorCode::StorageFailed, "Something went wrong. Nothing was saved.", {}, e.what()));
    }
  }();
  if (result) bus_.publish(app::AgentChanged{*result});
  return result;
}

}  // namespace as::services
