#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace as::render {

struct Texture {
  SDL_Texture* tex = nullptr;  // owned by the TextureCache
  int w = 0;
  int h = 0;
  explicit operator bool() const { return tex != nullptr; }
};

// Loads PNGs through SDL3_image with nearest-neighbour scaling and keeps them until destruction.
class TextureCache {
 public:
  explicit TextureCache(SDL_Renderer* renderer) : renderer_(renderer) {}
  ~TextureCache();
  TextureCache(const TextureCache&) = delete;
  TextureCache& operator=(const TextureCache&) = delete;

  // Returns an empty Texture (and logs once) if the file can't be loaded.
  Texture load(const std::filesystem::path& path);

 private:
  SDL_Renderer* renderer_;
  std::map<std::string, Texture> cache_;
};

}  // namespace as::render
