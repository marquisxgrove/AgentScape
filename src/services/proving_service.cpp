#include "services/proving_service.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "core/civil_time.hpp"

namespace as::services {

namespace {
ProvingSelection cleaned(ProvingSelection s) {
  std::vector<std::string> out;
  for (auto& slug : s.slugs) {
    if (out.size() >= domain::MAX_COMPARE) break;
    if (slug.empty() || std::find(out.begin(), out.end(), slug) != out.end()) continue;
    out.push_back(std::move(slug));
  }
  s.slugs = std::move(out);
  return s;
}
}  // namespace

Result<ProvingSelection> ProvingService::remember(const std::string& userId, ProvingSelection selection) {
  return db::guarded([&]() -> Result<ProvingSelection> {
    selection = cleaned(std::move(selection));
    db::Transaction tx{db_};
    auto user = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind <> 'NPC'");
    user.bind(1, userId);
    if (!user.step()) return makeError(GameErrorCode::Forbidden, "You can't do that right now.");
    db_.prepare("INSERT INTO proving_selection(user_id, slugs, quest_type, updated_at) VALUES (?, ?, ?, ?) "
                "ON CONFLICT(user_id) DO UPDATE SET slugs = excluded.slugs, quest_type = excluded.quest_type, "
                "updated_at = excluded.updated_at")
        .bind(1, userId)
        .bind(2, nlohmann::json(selection.slugs).dump())
        .bind(3, selection.questType ? std::optional<std::string>{domain::storageName(*selection.questType)} : std::nullopt)
        .bind(4, core::formatIso8601(clock_.now()))
        .run();
    tx.commit();
    return selection;
  }).and_then([](auto r) { return r; });
}

Result<ProvingSelection> ProvingService::restore(const std::string& userId) {
  return db::guarded([&]() -> Result<ProvingSelection> {
    auto st = db_.prepare("SELECT slugs, quest_type FROM proving_selection WHERE user_id = ?");
    st.bind(1, userId);
    ProvingSelection out;
    if (!st.step()) return out;
    // Lenient: a damaged row just means "nothing remembered".
    const auto json = nlohmann::json::parse(st.columnText(0), nullptr, /*allow_exceptions=*/false);
    if (json.is_array()) {
      for (const auto& v : json) {
        if (v.is_string()) out.slugs.push_back(v.get<std::string>());
      }
    }
    if (const auto q = st.columnOptText(1)) out.questType = domain::questTypeFromStorage(*q);
    return cleaned(std::move(out));
  }).and_then([](auto r) { return r; });
}

}  // namespace as::services
