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
  // Scripted demo path for the headless smoke test (--smoke-demo). Returns true when finished.
  bool runDemoStep();
  bool runPathStep();      // --smoke-path: the documented demo path, and (phase 2) that it survived a restart
  bool runCodexStep();     // --smoke-codex: the agent-codex flow end to end (steps are listed in app.cpp)
  bool runCostumerStep();  // --smoke-costumer: create, claim, equip, save; phase 2 verifies after a restart
  void snapshotTo(const std::string& file);
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
