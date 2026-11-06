// SDL3 main callbacks only; all behaviour lives in app::App.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <memory>

#include "app/app.hpp"

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  SDL_SetAppMetadata("AgentScape", "0.1.0", "dev.agentscape.AgentScape");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  auto app = as::app::App::create(as::app::parseConfig(argc, argv));
  if (!app) {
    return SDL_APP_FAILURE;
  }
  *appstate = app.release();
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  return static_cast<as::app::App*>(appstate)->handleEvent(*event);
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  return static_cast<as::app::App*>(appstate)->iterate();
}

void SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) {
  std::unique_ptr<as::app::App> app{static_cast<as::app::App*>(appstate)};
  app.reset();
  SDL_Quit();
}
