#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <optional>
#include <string>

#include "app/config.hpp"
#include "app/local_store.hpp"
#include "app/screen_stack.hpp"
#include "app/services.hpp"
#include "core/rng.hpp"
#include "platform/persist.hpp"
#include "render/game_frame.hpp"
#include "render/texture_cache.hpp"
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
  ui::UiContext ui_;
  ScreenStack screens_;
  bool fontTextureNearest_ = false;
  bool kiroween_ = false;
  int framesRendered_ = 0;
  bool quitRequested_ = false;

  core::SystemRng rng_;
  core::SystemClock clock_;
  platform::Persistence persistence_;
  DataState dataState_ = DataState::WaitingForStorage;
  std::optional<OpenedStore> store_;
  std::optional<Services> services_;
  std::optional<std::string> currentUserId_;
  domain::Settings settings_;
  std::optional<LocalStoreError> loadError_;
  bool inMemoryOnly_ = false;  // web without IndexedDB: progress won't survive a reload
  std::string savePath_;
};

}  // namespace as::app
