#pragma once

#include <SDL3/SDL.h>

#include <imgui.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "app/config.hpp"
#include "app/local_store.hpp"
#include "app/screen_stack.hpp"
#include "app/context_request.hpp"
#include "app/event_bus.hpp"
#include "app/services.hpp"
#include "core/rng.hpp"
#include "game/avatar_renderer.hpp"
#include "game/effects.hpp"
#include "platform/png_export.hpp"
#include "platform/persist.hpp"
#include "render/game_frame.hpp"
#include "render/texture_cache.hpp"
#include "ui/chatbox.hpp"
#include "ui/context_menu_host.hpp"
#include "ui/tab_panel.hpp"
#include "ui/ui_context.hpp"

namespace as::app {

// Builds the Config from argv, the process environment and (web) the URL query.
Config configFromProcess(int argc, char** argv);

// The only class main.cpp touches. Owns the window, renderer, ImGui context, frame and Local_Store.
class App {
 public:
  static std::unique_ptr<App> create(const Config& config);
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;
  App(App&&) = delete;
  App& operator=(App&&) = delete;

  SDL_AppResult handleEvent(SDL_Event& event);
  SDL_AppResult iterate();

 private:
  enum class DataState { WaitingForStorage, Ready, LoadFailed };

  explicit App(Config config) : config_(std::move(config)) {}
  bool init();
  void updateData();
  void openStore(bool resetData);
  void onStoreReady();
  void signInRemembered();
  void adoptUser(const std::string& userId, const domain::Settings& settings);
  // Credits the signed-in user's running coding session (coding-sessions Requirement 2.3). `away`: on sign-in, so
  // the time spent away is reported in the Chatbox.
  void settleCoding(bool away);
  // Scripted smoke runs (smoke_runs.cpp), picked by a --smoke-* flag. Each step returns true when the run is over.
  const std::optional<std::string>& smokeDir() const;  // the selected run's snapshot directory; nullopt = no run
  bool runSmokeStep();     // the selected run's next step
  bool runDemoStep();      // --smoke-demo: guest login -> World -> the Tavern context menu
  bool runPathStep();      // --smoke-path: the documented demo path, and (phase 2) that it survived a restart
  bool runCodexStep();     // --smoke-codex: the agent-codex flow end to end (steps are listed in smoke_runs.cpp)
  bool runCostumerStep();  // --smoke-costumer: create, claim, equip, save; phase 2 verifies after a restart
  bool smokeFail(const char* why);   // logs the failure and ends the run (returns true)
  void smokeShot(const char* file);  // saves the current frame into smokeDir()
  void onXpChanged(const XpChanged& e);
  void onItemUnlocked(const ItemUnlocked& e);
  void drawEffects();
  ui::XpStatus xpStatus() const;
  void registerScreens();
  void applySettings(const domain::Settings& settings);
  void drawShell();
  void drawLoading();
  void drawLoadFailed();
  void saveSnapshot();

  Config config_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  std::unique_ptr<render::GameFrame> frame_;
  std::unique_ptr<render::TextureCache> textures_;
  std::unique_ptr<game::AvatarRenderer> avatars_;
  platform::PngExporter pngExporter_;
  ui::UiContext ui_;
  ScreenStack screens_;
  bool fontTextureNearest_ = false;
  bool kiroween_ = false;
  int framesRendered_ = 0;
  bool quitRequested_ = false;
  std::size_t smokeTombstones_ = 0;    // --smoke-codex bookkeeping
  std::int64_t smokeXpBefore_ = 0;
  int smokeLevelBefore_ = 1;
  std::string smokeQuestLogId_;
  int demoStep_ = 0;   // index into the --smoke-demo sequence
  bool demoFailed_ = false;

  core::SystemRng rng_;
  core::SystemClock clock_;
  app::EventBus bus_;
  services::CodexCache codexCache_{bus_};  // must outlive Services: CodexService holds a reference
  std::optional<ui::Chatbox> chatbox_;
  ui::ContextMenuHost contextMenu_;
  app::ContextRequest contextReq_;
  platform::Persistence persistence_;
  DataState dataState_ = DataState::WaitingForStorage;
  std::optional<OpenedStore> store_;
  std::optional<Services> services_;
  std::optional<std::string> currentUserId_;
  ScreenParams postLoginParams_;
  std::optional<ScreenId> postLoginScreen_;  // dev: --screen=costumer opens this right after sign-in
  bool isKeeper_ = false;  // cached KEEPER role of the signed-in user
  bool isGuest_ = false;   // the signed-in user is a Guest (Log out asks first)
  double codingSettleIn_ = 0;   // seconds until the next coding-session settle
  int currentLevel_ = 1;               // the signed-in user's Questing level, from their XP
  std::int64_t totalXp_ = 0;           // and their total XP (kept in step by XpChanged events)
  game::Effects effects_;              // floating "+N XP" drops and level-up fireworks
  ImVec2 xpAnchor_{0, 0};              // where the tab panel's level line was drawn last frame
  app::Subscription xpSub_;
  app::Subscription unlockSub_;
  domain::Settings settings_;
  std::optional<LocalStoreError> loadError_;
  bool inMemoryOnly_ = false;  // web without IndexedDB: progress won't survive a reload
  std::string savePath_;
};

}  // namespace as::app
