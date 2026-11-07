#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain/appearance.hpp"
#include "domain/context_menu.hpp"
#include "domain/loadout.hpp"

namespace as::app {

enum class ScreenId { Login, World, Costumer, Codex, CodexCard, Propose, Proving, QuestScroll, QuestDetail, Tavern, Hiscores, Graveyard, Keeper, DevGallery, DevAvatars };

// Navigation parameters, e.g. {"agent", "<slug>"} when opening an examine card.
using ScreenParams = std::map<std::string, std::string>;

class ScreenStack;  // forward
}  // namespace as::app
namespace as::game {
class AvatarRenderer;
}  // namespace as::game
namespace as::app {

// Per-frame context handed to screens. `services` is set by App; screens never touch the db directly.
struct FrameCtx {
  double dt = 0.0;
  bool kiroween = false;
  bool reduceMotion = false;
  struct Services* services = nullptr;
  ScreenStack* nav = nullptr;
  struct ContextRequest* contextRequest = nullptr;  // a screen fills this to open a context menu
  game::AvatarRenderer* avatars = nullptr;          // paper-doll renderer (null if the atlas failed to load)

  // The signed-in user. userLevel is the Questing level (a placeholder until the quest-logs spec lands:
  // Guests start at 13, everyone else at 1).
  std::string userId;
  int userLevel = 1;
  bool isKeeper = false;  // the user holds the KEEPER role (Settings > Codex Keeper tools)
  // Starts a 4x transparent PNG export of a look (desktop save dialog / web download).
  std::function<void(const domain::Appearance&, const domain::Loadout&, domain::Facing, const std::string& name)>
      exportPortrait;

  // Default "Reduce motion" for a NEW guest (web: the browser preference). Desktop: false.
  bool reduceMotionDefault = false;
  // A screen calls this after a successful sign-in so App can adopt the user (remember settings,
  // apply the font, optionally remember the profile). userId is a ULID; remember is for real players.
  std::function<void(const std::string& userId, bool remember)> onSignedIn;
};

// Tabs on the TabPanel a screen may force (e.g. the Costumer shows Equipment).
enum class Tab { Equipment, Skills, QuestLog, Settings };

class Screen {
 public:
  virtual ~Screen() = default;
  virtual ScreenId id() const = 0;
  virtual void onEnter(const ScreenParams& /*params*/) {}
  virtual void onExit() {}
  virtual void update(FrameCtx& /*ctx*/) {}
  virtual void draw(FrameCtx& ctx) = 0;
  virtual std::optional<Tab> tabOverride() const { return std::nullopt; }
  // A non-Examine choice from a Context_Menu this screen opened (e.g. "remove:Head").
  virtual void onContextAction(FrameCtx& /*ctx*/, const domain::MenuAction& /*action*/) {}
};

using ScreenFactory = std::function<std::unique_ptr<Screen>()>;

// History of screens. navigate() pushes (or, for the hub screens Login/World, resets the stack),
// back() pops. Transitions requested during update/draw are applied at the next applyPending().
class ScreenStack {
 public:
  void registerScreen(ScreenId id, ScreenFactory factory);

  void navigate(ScreenId id, ScreenParams params = {});
  void back();
  void applyPending();  // call once per frame, outside of update/draw

  Screen* current();
  std::optional<ScreenId> currentId() const;
  std::size_t depth() const { return stack_.size(); }

 private:
  struct Pending {
    enum class Kind { Navigate, Back } kind;
    ScreenId id;
    ScreenParams params;
  };
  void doNavigate(ScreenId id, const ScreenParams& params);
  void doBack();

  std::map<ScreenId, ScreenFactory> factories_;
  std::vector<std::unique_ptr<Screen>> stack_;
  std::vector<Pending> pending_;
};

}  // namespace as::app
