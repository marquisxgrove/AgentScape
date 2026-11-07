#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "core/rng.hpp"
#include "db/database.hpp"
#include "domain/settings.hpp"

namespace as::services {

enum class UserKind { Player, Guest, Npc };
enum class UserRole { User, Keeper };

struct User {
  std::string id;  // ULID
  std::string name;
  UserKind kind = UserKind::Player;
  UserRole role = UserRole::User;
  domain::Settings settings;
  std::chrono::sys_seconds createdAt;
  std::chrono::sys_seconds lastSeenAt;
};

inline constexpr std::chrono::hours GUEST_MAX_IDLE{24};

// Local profiles (game-shell Requirements 2 and 7.1). Every write is one transaction.
// Guests are never "remembered": the app only auto-signs-in a real (non-Guest) User.
class ProfileService {
 public:
  ProfileService(db::Database& db, const core::Clock& clock, core::Rng& rng) : db_(db), clock_(clock), rng_(rng) {}

  // Fresh, isolated Guest named Guest-XXXX. `defaults` seeds its settings (web: reduceMotion from
  // prefers-reduced-motion). NOTE: quest-logs Req 9.1 also seeds Guest XP; the XP column arrives with
  // that spec's migration, and this is the single place to add it.
  Result<User> createGuest(const domain::Settings& defaults = {});

  // Signs in the PLAYER linked to this GitHub id, creating one on first use. Updates the stored
  // login/name on every sign-in. The caller has already discarded the access token.
  Result<User> signInGitHub(std::int64_t githubId, const std::string& login, const std::string& displayName);

  Result<User> getUser(const std::string& id);
  // Marks the user as active now (keeps Guests from being cleaned up) and returns the fresh row.
  Result<User> signIn(const std::string& id);

  // Selectable "continue as" profiles: PLAYER users only, most recently used first.
  Result<std::vector<User>> listLocalUsers();

  // Only PLAYER users can be remembered. Guests and NPCs are rejected.
  Result<void> rememberUser(const std::string& id);
  Result<void> forgetRemembered();
  Result<std::optional<User>> rememberedUser();  // nullopt if none, or if it no longer exists

  Result<User> updateSettings(const std::string& id, const domain::Settings& settings);

  // Called for each idle Guest just BEFORE its rows are deleted, so its content leaves the Codex through the normal
  // delete paths (ratings out of the aggregates, helpful and note votes withdrawn) instead of being orphaned by
  // ON DELETE CASCADE. App wires this to QuestService::purgeUser and NoteService::retractAllVotes.
  using BeforeGuestDelete = std::function<void(const std::string& userId)>;
  void setBeforeGuestDelete(BeforeGuestDelete hook) { beforeGuestDelete_ = std::move(hook); }

  // Deletes Guests idle for more than GUEST_MAX_IDLE (and, via ON DELETE CASCADE, their content).
  // Returns how many were removed.
  Result<int> cleanupGuests();

 private:
  db::Database& db_;
  const core::Clock& clock_;
  core::Rng& rng_;
  BeforeGuestDelete beforeGuestDelete_;
};

}  // namespace as::services
