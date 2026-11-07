#include "services/profile_service.hpp"

#include "core/civil_time.hpp"
#include "core/ids.hpp"
#include "domain/adventurer_name.hpp"
#include "domain/guest_name.hpp"
#include "domain/xp.hpp"

namespace as::services {

namespace {
constexpr const char* USER_COLUMNS = "id, name, kind, role, settings, created_at, last_seen_at, total_xp";
constexpr const char* REMEMBERED_KEY = "remembered_user_id";
// Guests idle since ?1, except the one remembered on this device (app_state key ?2): it lives until Log out.
constexpr const char* IDLE_GUEST =
    "kind = 'GUEST' AND last_seen_at < ?1 AND id NOT IN (SELECT value FROM app_state WHERE key = ?2)";

UserKind kindFrom(const std::string& s) {
  if (s == "GUEST") return UserKind::Guest;
  if (s == "NPC") return UserKind::Npc;
  return UserKind::Player;
}

std::chrono::sys_seconds timeFrom(const std::string& s) {
  return core::parseIso8601(s).value_or(std::chrono::sys_seconds{});
}

User readUser(const db::Statement& st) {
  User u;
  u.id = st.columnText(0);
  u.name = st.columnText(1);
  u.kind = kindFrom(st.columnText(2));
  u.role = st.columnText(3) == "KEEPER" ? UserRole::Keeper : UserRole::User;
  u.settings = domain::settingsFromJson(st.columnText(4));
  u.createdAt = timeFrom(st.columnText(5));
  u.lastSeenAt = timeFrom(st.columnText(6));
  u.totalXp = st.columnInt(7);
  return u;
}

std::optional<User> findUser(db::Database& db, const std::string& id) {
  auto st = db.prepare(std::string("SELECT ") + USER_COLUMNS + " FROM user WHERE id = ?");
  st.bind(1, id);
  if (!st.step()) return std::nullopt;
  return readUser(st);
}

Result<User> notFound() { return makeError(GameErrorCode::NotFound, "You can't do that right now."); }

constexpr const char* MSG_BAD_NAME = "Use 1 to 12 letters or numbers, with single spaces, - or _ between them.";
constexpr const char* MSG_NAME_TAKEN = "That name is not available.";
}  // namespace

Result<User> ProfileService::createPlayer(std::string_view rawName, const domain::Settings& defaults) {
  const auto name = domain::validateName(rawName, domain::RESERVED_NAMES);
  if (!name) {
    return makeError(GameErrorCode::InvalidInput,
                     name.error() == domain::NameError::Blocked ? MSG_NAME_TAKEN : MSG_BAD_NAME);
  }
  const std::string key = domain::nameKey(*name);
  if (key.rfind("guest ", 0) == 0) return makeError(GameErrorCode::InvalidInput, MSG_NAME_TAKEN);  // reads as a Guest

  auto created = db::guarded([&]() -> std::optional<User> {
    const auto now = clock_.now();
    const std::string stamp = core::formatIso8601(now);

    db::Transaction tx{db_};
    // Players and the seeded townsfolk share one namespace. The table has no name key column, and there are only a
    // handful of local profiles, so compare keys here.
    auto others = db_.prepare("SELECT name FROM user WHERE kind IN ('PLAYER', 'NPC')");
    while (others.step()) {
      if (domain::nameKey(others.columnText(0)) == key) return std::nullopt;
    }
    User u;
    u.id = core::newUlid(now, rng_);
    u.name = *name;
    u.kind = UserKind::Player;
    u.settings = defaults;
    u.createdAt = now;
    u.lastSeenAt = now;
    db_.prepare(
           "INSERT INTO user(id, name, kind, role, settings, created_at, last_seen_at) "
           "VALUES (?, ?, 'PLAYER', 'USER', ?, ?, ?)")
        .bind(1, u.id)
        .bind(2, u.name)
        .bind(3, domain::settingsToJson(defaults))
        .bind(4, stamp)
        .bind(5, stamp)
        .run();
    tx.commit();
    return u;
  });
  if (!created) return tl::unexpected(created.error());
  if (!*created) return makeError(GameErrorCode::InvalidInput, MSG_NAME_TAKEN);
  return **created;
}

Result<User> ProfileService::createGuest(const domain::Settings& defaults) {
  return db::guarded([&]() -> User {
    const auto now = clock_.now();
    const std::string stamp = core::formatIso8601(now);
    const std::string id = core::newUlid(now, rng_);
    const std::string name = domain::randomGuestName(rng_);

    db::Transaction tx{db_};
    db_.prepare(
           "INSERT INTO user(id, name, kind, role, settings, created_at, last_seen_at) "
           "VALUES (?, ?, 'GUEST', 'USER', ?, ?, ?)")
        .bind(1, id)
        .bind(2, name)
        .bind(3, domain::settingsToJson(defaults))
        .bind(4, stamp)
        .bind(5, stamp)
        .run();
    // A Guest starts at Questing level 13 (quest-logs Requirement 9.1): every early unlock and the Reaper's Scythe are
    // claimable, and the first Quest Log is a level-up.
    db_.prepare("INSERT INTO xp_entry(id, user_id, reason, ref_id, amount, created_at) VALUES (?, ?, 'SEED', 'seed', ?, ?)")
        .bind(1, core::newUlid(now, rng_))
        .bind(2, id)
        .bind(3, domain::guestSeedXp())
        .bind(4, stamp)
        .run();
    db_.prepare("UPDATE user SET total_xp = ? WHERE id = ?").bind(1, domain::guestSeedXp()).bind(2, id).run();
    tx.commit();

    User u;
    u.id = id;
    u.name = name;
    u.kind = UserKind::Guest;
    u.settings = defaults;
    u.createdAt = now;
    u.lastSeenAt = now;
    return u;
  });
}

Result<User> ProfileService::signInGitHub(std::int64_t githubId, const std::string& login,
                                          const std::string& displayName) {
  return db::guarded([&]() -> std::optional<User> {
           const auto now = clock_.now();
           const std::string stamp = core::formatIso8601(now);

           db::Transaction tx{db_};
           auto existing = db_.prepare("SELECT id FROM user WHERE github_id = ?");
           existing.bind(1, githubId);
           std::string id;
           if (existing.step()) {
             id = existing.columnText(0);
             db_.prepare("UPDATE user SET name = ?, github_login = ?, last_seen_at = ? WHERE id = ?")
                 .bind(1, displayName)
                 .bind(2, login)
                 .bind(3, stamp)
                 .bind(4, id)
                 .run();
           } else {
             id = core::newUlid(now, rng_);
             db_.prepare(
                    "INSERT INTO user(id, name, kind, role, github_id, github_login, settings, created_at, "
                    "last_seen_at) VALUES (?, ?, 'PLAYER', 'USER', ?, ?, '{}', ?, ?)")
                 .bind(1, id)
                 .bind(2, displayName)
                 .bind(3, githubId)
                 .bind(4, login)
                 .bind(5, stamp)
                 .bind(6, stamp)
                 .run();
           }
           tx.commit();
           return findUser(db_, id);
         })
      .and_then([](std::optional<User> u) -> Result<User> {
        if (!u) return notFound();
        return *u;
      });
}

Result<User> ProfileService::getUser(const std::string& id) {
  auto found = db::guarded([&] { return findUser(db_, id); });
  if (!found) return tl::unexpected(found.error());
  if (!*found) return notFound();
  return **found;
}

Result<User> ProfileService::signIn(const std::string& id) {
  return db::guarded([&]() -> std::optional<User> {
           db::Transaction tx{db_};
           db_.prepare("UPDATE user SET last_seen_at = ? WHERE id = ? AND kind != 'NPC'")
               .bind(1, core::formatIso8601(clock_.now()))
               .bind(2, id)
               .run();
           const bool updated = db_.changes() == 1;
           tx.commit();
           return updated ? findUser(db_, id) : std::nullopt;
         })
      .and_then([](std::optional<User> u) -> Result<User> {
        if (!u) return notFound();
        return *u;
      });
}

Result<std::vector<User>> ProfileService::listLocalUsers() {
  return db::guarded([&] {
    std::vector<User> users;
    auto st = db_.prepare(std::string("SELECT ") + USER_COLUMNS +
                          " FROM user WHERE kind = 'PLAYER' ORDER BY last_seen_at DESC, name ASC");
    while (st.step()) users.push_back(readUser(st));
    return users;
  });
}

Result<void> ProfileService::rememberUser(const std::string& id) {
  auto r = db::guarded([&]() -> bool {
    db::Transaction tx{db_};
    auto check = db_.prepare("SELECT 1 FROM user WHERE id = ? AND kind IN ('PLAYER', 'GUEST')");
    check.bind(1, id);
    if (!check.step()) return false;  // missing, or an NPC
    db_.prepare("INSERT INTO app_state(key, value) VALUES (?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value = excluded.value")
        .bind(1, REMEMBERED_KEY)
        .bind(2, id)
        .run();
    tx.commit();
    return true;
  });
  if (!r) return tl::unexpected(r.error());
  if (!*r) return makeError(GameErrorCode::Forbidden, "You can't do that right now.");
  return {};
}

Result<void> ProfileService::forgetRemembered() {
  return db::guarded([&] {
    db::Transaction tx{db_};
    db_.prepare("DELETE FROM app_state WHERE key = ?").bind(1, REMEMBERED_KEY).run();
    tx.commit();
  });
}

Result<std::optional<User>> ProfileService::rememberedUser() {
  return db::guarded([&]() -> std::optional<User> {
    auto st = db_.prepare("SELECT value FROM app_state WHERE key = ?");
    st.bind(1, REMEMBERED_KEY);
    if (!st.step()) return std::nullopt;
    auto user = findUser(db_, st.columnText(0));
    // Defensive: an NPC is never auto-signed-in.
    if (user && user->kind == UserKind::Npc) return std::nullopt;
    return user;
  });
}

Result<User> ProfileService::updateSettings(const std::string& id, const domain::Settings& settings) {
  return db::guarded([&]() -> std::optional<User> {
           db::Transaction tx{db_};
           db_.prepare("UPDATE user SET settings = ?, last_seen_at = ? WHERE id = ? AND kind != 'NPC'")
               .bind(1, domain::settingsToJson(settings))
               .bind(2, core::formatIso8601(clock_.now()))
               .bind(3, id)
               .run();
           const bool updated = db_.changes() == 1;
           tx.commit();
           return updated ? findUser(db_, id) : std::nullopt;
         })
      .and_then([](std::optional<User> u) -> Result<User> {
        if (!u) return notFound();
        return *u;
      });
}

Result<int> ProfileService::cleanupGuests() {
  const auto cutoff = clock_.now() - GUEST_MAX_IDLE;
  if (beforeGuestDelete_) {
    std::vector<std::string> idle;
    const auto listed = db::guarded([&] {
      auto st = db_.prepare(std::string("SELECT id FROM user WHERE ") + IDLE_GUEST);
      st.bind(1, core::formatIso8601(cutoff));
      st.bind(2, REMEMBERED_KEY);
      while (st.step()) idle.push_back(st.columnText(0));
    });
    if (!listed) return tl::unexpected(listed.error());
    for (const auto& id : idle) beforeGuestDelete_(id);   // each hook call is its own transaction
  }
  return db::guarded([&] {
    db::Transaction tx{db_};
    db_.prepare(std::string("DELETE FROM user WHERE ") + IDLE_GUEST)
        .bind(1, core::formatIso8601(cutoff))
        .bind(2, REMEMBERED_KEY)
        .run();
    const int removed = db_.changes();
    // A remembered pointer must never dangle.
    db_.exec("DELETE FROM app_state WHERE key = 'remembered_user_id' "
             "AND value NOT IN (SELECT id FROM user)");
    tx.commit();
    return removed;
  });
}

}  // namespace as::services
