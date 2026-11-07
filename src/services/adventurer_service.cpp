#include "services/adventurer_service.hpp"

#include <array>
#include <nlohmann/json.hpp>

#include "core/civil_time.hpp"
#include "core/ids.hpp"
#include "domain/adventurer_name.hpp"
#include "domain/appearance_json.hpp"
#include "domain/loadout_json.hpp"
#include "domain/wardrobe.hpp"

namespace as::services {

namespace {

constexpr const char* COLUMNS = "id, user_id, name, appearance, loadout, is_active, created_at, updated_at";

constexpr const char* MSG_NAME_TAKEN = "That name is not available.";
constexpr const char* MSG_TOO_MANY = "You can't have more than 5 Adventurers.";
constexpr const char* MSG_LOCKED = "You don't have that item yet.";
constexpr const char* MSG_GENERIC = "You can't do that right now.";

std::chrono::sys_seconds timeFrom(const std::string& s) {
  return core::parseIso8601(s).value_or(std::chrono::sys_seconds{});
}

domain::OwnedSet loadOwned(db::Database& db, const std::string& userId) {
  domain::OwnedSet owned = domain::defaultOwnedSet();
  auto st = db.prepare("SELECT item_id FROM item_unlock WHERE user_id = ?");
  st.bind(1, userId);
  while (st.step()) owned.insert(st.columnText(0));
  return owned;
}

// Reads one row, repairing stored data that no longer validates (never throws).
Adventurer readRow(const db::Statement& st, const domain::OwnedSet& owned) {
  Adventurer a;
  a.id = st.columnText(0);
  a.userId = st.columnText(1);
  a.name = st.columnText(2);
  a.isActive = st.columnInt(5) == 1;
  a.createdAt = timeFrom(st.columnText(6));
  a.updatedAt = timeFrom(st.columnText(7));

  const auto parsedAppearance = domain::parseAppearance(st.columnText(3));
  if (parsedAppearance && domain::validateAppearance(*parsedAppearance)) {
    a.appearance = *parsedAppearance;
  } else {
    a.appearance = domain::Appearance{};
    a.repaired = true;
  }

  const std::string loadoutText = st.columnText(4);
  const auto loadoutJson = nlohmann::json::parse(loadoutText, nullptr, /*allow_exceptions=*/false);
  domain::Loadout stored;
  if (loadoutJson.is_object()) {
    domain::from_json(loadoutJson, stored);
  } else {
    a.repaired = true;
  }
  a.loadout = domain::validateLoadout(stored, owned);
  if (a.loadout != stored) a.repaired = true;
  return a;
}

std::optional<Adventurer> findOwned(db::Database& db, const std::string& userId, const std::string& id) {
  auto st = db.prepare(std::string("SELECT ") + COLUMNS + " FROM adventurer WHERE id = ? AND user_id = ?");
  st.bind(1, id).bind(2, userId);
  if (!st.step()) return std::nullopt;
  return readRow(st, loadOwned(db, userId));
}

int countFor(db::Database& db, const std::string& userId) {
  auto st = db.prepare("SELECT COUNT(*) FROM adventurer WHERE user_id = ?");
  st.bind(1, userId);
  st.step();
  return static_cast<int>(st.columnInt(0));
}

Result<Adventurer> notFound() { return makeError(GameErrorCode::NotFound, MSG_GENERIC); }

}  // namespace

Result<Adventurer> AdventurerService::createAdventurer(const std::string& userId, std::string_view rawName) {
  const auto name = domain::validateName(rawName, domain::RESERVED_NAMES);
  if (!name) return makeError(GameErrorCode::InvalidInput, MSG_NAME_TAKEN);

  struct Outcome {
    std::optional<Adventurer> adventurer;
    const char* error = nullptr;
    GameErrorCode code = GameErrorCode::InvalidInput;
  };
  auto outcome = db::guarded([&]() -> Outcome {
    const auto now = clock_.now();
    const std::string stamp = core::formatIso8601(now);
    const std::string key = domain::nameKey(*name);

    db::Transaction tx{db_};
    {  // the owner must exist (a missing user would otherwise surface as a foreign-key error)
      auto owner = db_.prepare("SELECT 1 FROM user WHERE id = ?");
      owner.bind(1, userId);
      if (!owner.step()) return {std::nullopt, MSG_GENERIC, GameErrorCode::NotFound};
    }
    if (countFor(db_, userId) >= MAX_ADVENTURERS) return {std::nullopt, MSG_TOO_MANY, GameErrorCode::Forbidden};
    {
      auto taken = db_.prepare("SELECT 1 FROM adventurer WHERE name_key = ?");
      taken.bind(1, key);
      if (taken.step()) return {std::nullopt, MSG_NAME_TAKEN, GameErrorCode::InvalidInput};
    }
    const bool first = countFor(db_, userId) == 0;  // Req 1.5: the first Adventurer is active

    Adventurer a;
    a.id = core::newUlid(now, rng_);
    a.userId = userId;
    a.name = *name;
    a.appearance = domain::Appearance{};
    a.loadout = domain::defaultLoadout();
    a.isActive = first;
    a.createdAt = a.updatedAt = now;

    const nlohmann::json appearanceJson = a.appearance;
    const nlohmann::json loadoutJson = a.loadout;
    db_.prepare(
           "INSERT INTO adventurer(id, user_id, name, name_key, appearance, loadout, is_active, created_at, "
           "updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)")
        .bind(1, a.id)
        .bind(2, userId)
        .bind(3, a.name)
        .bind(4, key)
        .bind(5, appearanceJson.dump())
        .bind(6, loadoutJson.dump())
        .bind(7, a.isActive ? 1 : 0)
        .bind(8, stamp)
        .bind(9, stamp)
        .run();
    tx.commit();
    return {a, nullptr, GameErrorCode::Internal};
  });
  if (!outcome) return tl::unexpected(outcome.error());
  if (!outcome->adventurer) return makeError(outcome->code, outcome->error);
  return *outcome->adventurer;
}

Result<Adventurer> AdventurerService::saveAdventurer(const std::string& userId, const std::string& id,
                                                     const domain::Appearance& appearance,
                                                     const domain::Loadout& loadout) {
  if (!domain::validateAppearance(appearance)) return makeError(GameErrorCode::InvalidInput, MSG_GENERIC);

  struct Outcome {
    std::optional<Adventurer> adventurer;
    const char* error = nullptr;
    GameErrorCode code = GameErrorCode::InvalidInput;
  };
  auto outcome = db::guarded([&]() -> Outcome {
    db::Transaction tx{db_};
    if (!findOwned(db_, userId, id)) return {std::nullopt, MSG_GENERIC, GameErrorCode::NotFound};

    // validateLoadout drops unknown, mis-slotted, unowned items and resolves two-handed conflicts. Any
    // difference means the client sent something the rules don't allow.
    const auto owned = loadOwned(db_, userId);
    if (domain::validateLoadout(loadout, owned) != loadout) {
      return {std::nullopt, MSG_LOCKED, GameErrorCode::Forbidden};
    }

    const std::string stamp = core::formatIso8601(clock_.now());
    const nlohmann::json appearanceJson = appearance;
    const nlohmann::json loadoutJson = loadout;
    db_.prepare("UPDATE adventurer SET appearance = ?, loadout = ?, updated_at = ? WHERE id = ? AND user_id = ?")
        .bind(1, appearanceJson.dump())
        .bind(2, loadoutJson.dump())
        .bind(3, stamp)
        .bind(4, id)
        .bind(5, userId)
        .run();
    auto saved = findOwned(db_, userId, id);
    tx.commit();
    return {saved, nullptr, GameErrorCode::Internal};
  });
  if (!outcome) return tl::unexpected(outcome.error());
  if (!outcome->adventurer) return makeError(outcome->code, outcome->error);
  return *outcome->adventurer;
}

Result<Adventurer> AdventurerService::setActiveAdventurer(const std::string& userId, const std::string& id) {
  auto result = db::guarded([&]() -> std::optional<Adventurer> {
    db::Transaction tx{db_};
    if (!findOwned(db_, userId, id)) return std::nullopt;
    // Unset first, then set: the partial unique index forbids two active rows even transiently.
    db_.prepare("UPDATE adventurer SET is_active = 0 WHERE user_id = ? AND is_active = 1").bind(1, userId).run();
    db_.prepare("UPDATE adventurer SET is_active = 1 WHERE id = ? AND user_id = ?").bind(1, id).bind(2, userId).run();
    auto active = findOwned(db_, userId, id);
    tx.commit();
    return active;
  });
  if (!result) return tl::unexpected(result.error());
  if (!*result) return notFound();
  return **result;
}

Result<void> AdventurerService::deleteAdventurer(const std::string& userId, const std::string& id,
                                                 std::string_view confirmName) {
  enum class Outcome { Deleted, NotFound, WrongName };
  auto result = db::guarded([&]() -> Outcome {
    db::Transaction tx{db_};
    const auto target = findOwned(db_, userId, id);
    if (!target) return Outcome::NotFound;
    if (domain::nameKey(confirmName) != domain::nameKey(target->name)) return Outcome::WrongName;

    db_.prepare("DELETE FROM adventurer WHERE id = ? AND user_id = ?").bind(1, id).bind(2, userId).run();
    if (target->isActive) {
      // Promote the most recently updated remaining Adventurer (ties: newest id).
      auto next = db_.prepare(
          "SELECT id FROM adventurer WHERE user_id = ? ORDER BY updated_at DESC, id DESC LIMIT 1");
      next.bind(1, userId);
      if (next.step()) {
        db_.prepare("UPDATE adventurer SET is_active = 1 WHERE id = ?").bind(1, next.columnText(0)).run();
      }
    }
    tx.commit();
    return Outcome::Deleted;
  });
  if (!result) return tl::unexpected(result.error());
  switch (*result) {
    case Outcome::Deleted: return {};
    case Outcome::WrongName: return makeError(GameErrorCode::InvalidInput, "That is not the right name.");
    case Outcome::NotFound: break;
  }
  return makeError(GameErrorCode::NotFound, MSG_GENERIC);
}

Result<void> AdventurerService::claimKiroweenItem(const std::string& userId, std::string_view itemId,
                                                  bool kiroweenActive, int userLevel) {
  const domain::Item* item = domain::findItem(itemId);
  if (item == nullptr || !domain::isKiroween(*item)) return makeError(GameErrorCode::InvalidInput, MSG_GENERIC);
  if (!kiroweenActive) return makeError(GameErrorCode::Forbidden, "Returns next Kiroween.");
  const auto& rule = std::get<domain::UnlockKiroween>(item->unlock);
  if (rule.minLevel && userLevel < *rule.minLevel) {
    return makeError(GameErrorCode::Forbidden, "Requires Questing level " + std::to_string(*rule.minLevel) + ".");
  }
  return grantItem(userId, itemId, "KIROWEEN");
}

Result<void> AdventurerService::grantItem(const std::string& userId, std::string_view itemId,
                                          std::string_view source) {
  if (domain::findItem(itemId) == nullptr) return makeError(GameErrorCode::InvalidInput, MSG_GENERIC);
  if (source != "LEVEL" && source != "KIROWEEN" && source != "GRANT") {
    return makeError(GameErrorCode::InvalidInput, MSG_GENERIC);
  }
  return db::guarded([&] {
    db::Transaction tx{db_};
    db_.prepare("INSERT INTO item_unlock(user_id, item_id, source, unlocked_at) VALUES (?, ?, ?, ?) "
                "ON CONFLICT(user_id, item_id) DO NOTHING")
        .bind(1, userId)
        .bind(2, itemId)
        .bind(3, source)
        .bind(4, core::formatIso8601(clock_.now()))
        .run();
    tx.commit();
  });
}

Result<std::vector<std::string>> AdventurerService::syncLevelUnlocks(const std::string& userId, int level) {
  return db::guarded([&] {
    std::vector<std::string> granted;
    const std::string stamp = core::formatIso8601(clock_.now());
    db::Transaction tx{db_};
    for (const std::string& id : domain::levelUnlockIds(level)) {
      db_.prepare("INSERT INTO item_unlock(user_id, item_id, source, unlocked_at) VALUES (?, ?, 'LEVEL', ?) "
                  "ON CONFLICT(user_id, item_id) DO NOTHING")
          .bind(1, userId)
          .bind(2, id)
          .bind(3, stamp)
          .run();
      if (db_.changes() == 1) granted.push_back(id);
    }
    tx.commit();
    return granted;
  });
}

Result<std::vector<Adventurer>> AdventurerService::list(const std::string& userId) {
  return db::guarded([&] {
    std::vector<Adventurer> out;
    const auto owned = loadOwned(db_, userId);
    auto st = db_.prepare(std::string("SELECT ") + COLUMNS +
                          " FROM adventurer WHERE user_id = ? ORDER BY created_at ASC, id ASC");
    st.bind(1, userId);
    while (st.step()) out.push_back(readRow(st, owned));
    return out;
  });
}

Result<std::optional<Adventurer>> AdventurerService::getActive(const std::string& userId) {
  return db::guarded([&]() -> std::optional<Adventurer> {
    auto st = db_.prepare(std::string("SELECT ") + COLUMNS + " FROM adventurer WHERE user_id = ? AND is_active = 1");
    st.bind(1, userId);
    if (!st.step()) return std::nullopt;
    return readRow(st, loadOwned(db_, userId));
  });
}

Result<domain::OwnedSet> AdventurerService::ownedItems(const std::string& userId) {
  return db::guarded([&] { return loadOwned(db_, userId); });
}

}  // namespace as::services
