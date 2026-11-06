#include "render/texture_cache.hpp"

#include <SDL3_image/SDL_image.h>

namespace as::render {

TextureCache::~TextureCache() {
  for (auto& [path, t] : cache_) {
    if (t.tex != nullptr) SDL_DestroyTexture(t.tex);
  }
}

Texture TextureCache::load(const std::filesystem::path& path) {
  const auto key = path.string();
  if (const auto it = cache_.find(key); it != cache_.end()) return it->second;

  Texture t;
  t.tex = IMG_LoadTexture(renderer_, key.c_str());
  if (t.tex == nullptr) {
    SDL_Log("Could not load texture %s: %s", key.c_str(), SDL_GetError());
  } else {
    SDL_SetTextureScaleMode(t.tex, SDL_SCALEMODE_NEAREST);
    float w = 0;
    float h = 0;
    SDL_GetTextureSize(t.tex, &w, &h);
    t.w = static_cast<int>(w);
    t.h = static_cast<int>(h);
  }
  cache_.emplace(key, t);  // cache failures too, so a missing file is logged once
  return t;
}

}  // namespace as::render
