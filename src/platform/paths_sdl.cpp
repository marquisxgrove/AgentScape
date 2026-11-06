#include <SDL3/SDL.h>

#include "platform/paths.hpp"

namespace as::platform {

std::filesystem::path defaultDataDir() {
#ifdef __EMSCRIPTEN__
  return std::filesystem::path{WEB_PERSIST_DIR};
#else
  char* pref = SDL_GetPrefPath("AgentScape", "AgentScape");
  if (pref == nullptr) {
    SDL_Log("SDL_GetPrefPath failed (%s); using the working directory", SDL_GetError());
    return std::filesystem::current_path();
  }
  std::filesystem::path dir{pref};
  SDL_free(pref);
  return dir;
#endif
}

std::filesystem::path assetsDir() {
#ifdef __EMSCRIPTEN__
  return std::filesystem::path{"/assets"};
#else
  const char* base = SDL_GetBasePath();  // owned by SDL in SDL3; do not free
  return std::filesystem::path{base != nullptr ? base : ""} / "assets";
#endif
}

}  // namespace as::platform
