#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <optional>

#include "domain/kiroween.hpp"
#include "render/game_frame.hpp"

namespace as::app {

struct Config {
  std::optional<domain::KiroweenSetting> kiroween;  // resolved CLI > env > URL > build default
  bool reseed = false;
  bool resetData = false;
  bool headlessSmoke = false;  // --smoke: run a few frames and quit (used by CI)
};

// Parses argv and the environment. The web URL query is read via platform code later.
Config parseConfig(int argc, char** argv);

// The only class main.cpp touches. Owns the window, renderer, ImGui context and frame.
class App {
 public:
  static std::unique_ptr<App> create(const Config& config);
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  SDL_AppResult handleEvent(SDL_Event& event);
  SDL_AppResult iterate();

 private:
  explicit App(const Config& config) : config_(config) {}
  bool init();
  void drawShell();

  Config config_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  std::unique_ptr<render::GameFrame> frame_;
  bool kiroween_ = false;
  int framesRendered_ = 0;
};

}  // namespace as::app
